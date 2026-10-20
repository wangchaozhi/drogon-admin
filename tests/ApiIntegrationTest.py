#!/usr/bin/env python3
"""Black-box authentication, RBAC and CORS regression test."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import sqlite3
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
from contextlib import closing
from pathlib import Path
from typing import Any


ALLOWED_ORIGIN = "https://admin.example.test"


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def request(
    base_url: str,
    method: str,
    path: str,
    body: Any = None,
    token: str | None = None,
    headers: dict[str, str] | None = None,
) -> tuple[int, Any, Any]:
    def parse(raw: bytes) -> Any:
        if not raw:
            return None
        try:
            return json.loads(raw)
        except (json.JSONDecodeError, UnicodeDecodeError):
            return raw.decode("utf-8", errors="replace")

    payload = None if body is None else json.dumps(body).encode("utf-8")
    request_headers = dict(headers or {})
    if body is not None:
        request_headers["Content-Type"] = "application/json"
    if token:
        request_headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(
        base_url + path,
        data=payload,
        headers=request_headers,
        method=method,
    )
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            raw = response.read()
            parsed = parse(raw)
            return response.status, parsed, response.headers
    except urllib.error.HTTPError as error:
        raw = error.read()
        parsed = parse(raw)
        return error.code, parsed, error.headers


def expect_status(
    response: tuple[int, Any, Any],
    expected: int,
    context: str,
) -> tuple[int, Any, Any]:
    if response[0] != expected:
        raise AssertionError(
            f"{context}: expected HTTP {expected}, got {response[0]} "
            f"body={response[1]!r}"
        )
    return response


def expect_ok(
    response: tuple[int, Any, Any],
    context: str,
) -> Any:
    expect_status(response, 200, context)
    body = response[1]
    if not isinstance(body, dict) or body.get("code") != 0:
        raise AssertionError(f"{context}: invalid success envelope {body!r}")
    return body.get("data")


def write_config(root: Path, port: int) -> None:
    config = {
        "listeners": [
            {"address": "127.0.0.1", "port": port, "https": False}
        ],
        "app": {
            "threads_num": 2,
            "enable_session": False,
            "document_root": "./",
            "upload_path": "uploads",
            "client_max_body_size": "10M",
            "log": {
                "log_path": "",
                "logfile_base_name": "drogon-admin-test",
                "log_size_limit": 1000000,
                "log_level": "WARN",
            },
            "run_as_daemon": False,
            "relaunch_on_error": False,
            "use_sendfile": False,
            "use_gzip": False,
            "server_header_field": "drogon-admin",
        },
        "db_clients": [
            {
                "name": "default",
                "rdbms": "sqlite3",
                "filename": "data/drogon-admin.db",
                "connection_number": 1,
                "is_fast": False,
                "timeout": -1.0,
            }
        ],
        "plugins": [
            {
                "name": "plugins::ConfigPlugin",
                "dependencies": [],
                "config": {},
            }
        ],
        "custom_config": {
            "environment": "production",
            "jwt_expire_seconds": 7200,
            "registration_mode": "first-user",
            "login_max_failures": 5,
            "login_lock_seconds": 60,
            "cors_allowed_origins": [ALLOWED_ORIGIN],
            "cors_allow_credentials": False,
            "cors_max_age_seconds": 600,
            "enable_hsts": True,
        },
    }
    (root / "config").mkdir(parents=True)
    (root / "config" / "config.json").write_text(
        json.dumps(config), encoding="utf-8"
    )


def prepare_legacy_database(root: Path) -> None:
    """Create the pre-auth-security users table to exercise migration 001."""
    data_dir = root / "data"
    data_dir.mkdir(parents=True)
    with closing(
        sqlite3.connect(data_dir / "drogon-admin.db")
    ) as database:
        database.execute(
            """
            CREATE TABLE users (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                email TEXT NOT NULL UNIQUE,
                created_at INTEGER NOT NULL
            )
            """
        )
        database.commit()


def verify_migration_state(root: Path) -> None:
    with closing(
        sqlite3.connect(root / "data" / "drogon-admin.db")
    ) as database:
        columns = {
            row[1] for row in database.execute("PRAGMA table_info(users)")
        }
        expected = {
            "password_hash",
            "disabled",
            "token_version",
            "failed_login_count",
            "locked_until",
        }
        if not expected.issubset(columns):
            raise AssertionError(
                f"legacy migration missing columns: {sorted(expected - columns)}"
            )
        versions = {
            row[0]
            for row in database.execute(
                "SELECT version FROM schema_migrations"
            )
        }
        if "001_user_auth_security" not in versions:
            raise AssertionError(
                f"migration version was not recorded: {sorted(versions)}"
            )


def wait_until_ready(base_url: str, process: subprocess.Popen[Any]) -> None:
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"server exited during startup: {process.returncode}")
        try:
            if request(base_url, "GET", "/api/health")[0] == 200:
                return
        except (OSError, TimeoutError):
            pass
        time.sleep(0.1)
    raise TimeoutError("server did not become ready within 20 seconds")


def stop_process(process: subprocess.Popen[Any]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def run_checks(base_url: str) -> None:
    preflight = expect_status(
        request(
            base_url,
            "OPTIONS",
            "/api/auth/login",
            headers={
                "Origin": ALLOWED_ORIGIN,
                "Access-Control-Request-Method": "POST",
                "Access-Control-Request-Headers": "authorization, content-type",
            },
        ),
        204,
        "allowed CORS preflight",
    )
    assert preflight[2].get("Access-Control-Allow-Origin") == ALLOWED_ORIGIN
    assert preflight[2].get("Access-Control-Max-Age") == "600"
    assert preflight[2].get("Strict-Transport-Security")
    assert preflight[2].get("X-Request-ID")

    expect_status(
        request(
            base_url,
            "OPTIONS",
            "/api/auth/login",
            headers={
                "Origin": "https://evil.example.test",
                "Access-Control-Request-Method": "POST",
                "Access-Control-Request-Headers": "content-type",
            },
        ),
        403,
        "disallowed CORS origin",
    )
    expect_status(
        request(
            base_url,
            "OPTIONS",
            "/api/auth/login",
            headers={
                "Origin": ALLOWED_ORIGIN,
                "Access-Control-Request-Method": "POST",
                "Access-Control-Request-Headers": "x-unsafe-header",
            },
        ),
        403,
        "disallowed CORS header",
    )

    health = expect_status(
        request(
            base_url,
            "GET",
            "/api/health",
            headers={
                "Origin": ALLOWED_ORIGIN,
                "X-Request-ID": "integration-request-123",
            },
        ),
        200,
        "health",
    )
    assert health[2].get("Access-Control-Allow-Origin") == ALLOWED_ORIGIN
    assert health[2].get("X-Content-Type-Options") == "nosniff"
    assert health[2].get("X-Request-ID") == "integration-request-123"

    ready = expect_ok(
        request(base_url, "GET", "/api/ready"),
        "database readiness",
    )
    assert ready["status"] == "ready"
    assert ready["database"] == "ok"
    assert int(ready["migration_count"]) >= 1

    metrics = expect_status(
        request(base_url, "GET", "/api/metrics"),
        200,
        "Prometheus metrics",
    )
    assert isinstance(metrics[1], str)
    assert 'drogon_admin_build_info{version="' in metrics[1]
    assert 'version="unknown"' not in metrics[1]
    assert "drogon_admin_http_requests_total" in metrics[1]
    assert "drogon_admin_http_request_duration_seconds_bucket" in metrics[1]
    assert metrics[2].get_content_type() == "text/plain"
    assert metrics[2].get("Cache-Control") == "no-store"

    not_found = expect_status(
        request(base_url, "GET", "/api/not-found"),
        404,
        "API 404 security headers",
    )
    assert not_found[2].get("X-Frame-Options") == "DENY"

    invalid_login = expect_status(
        request(base_url, "POST", "/api/auth/login", {}),
        400,
        "invalid login",
    )
    assert invalid_login[2].get("Cache-Control") == "no-store"

    admin_password = "Admin-Horse-2026!"
    operator_password = "Operator-Horse-2026!"
    new_operator_password = "Updated-Operator-2026!"
    self_changed_password = "Self-Changed-Operator-2026!"

    admin = expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/register",
            {
                "name": "Admin",
                "email": "admin@example.com",
                "password": admin_password,
            },
        ),
        "first administrator registration",
    )
    admin_id = int(admin["id"])
    expect_status(
        request(
            base_url,
            "POST",
            "/api/auth/register",
            {
                "name": "Second",
                "email": "second@example.com",
                "password": "Second-Horse-2026!",
            },
        ),
        403,
        "registration closes after first administrator",
    )

    admin_login = expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {"email": "admin@example.com", "password": admin_password},
        ),
        "administrator login",
    )
    admin_token = str(admin_login["token"])
    me = expect_ok(
        request(base_url, "GET", "/api/users/me", token=admin_token),
        "authenticated profile",
    )
    assert int(me["user"]["id"]) == admin_id
    expect_status(
        request(
            base_url,
            "GET",
            "/api/users?page=1x",
            token=admin_token,
        ),
        400,
        "reject malformed pagination",
    )
    expect_status(
        request(
            base_url,
            "GET",
            "/api/users?page_size=201",
            token=admin_token,
        ),
        400,
        "reject excessive page size",
    )

    operator = expect_ok(
        request(
            base_url,
            "POST",
            "/api/users",
            {
                "name": "Operator",
                "email": "operator@example.com",
                "password": operator_password,
            },
            token=admin_token,
        ),
        "create operator",
    )
    operator_id = int(operator["id"])

    role = expect_ok(
        request(
            base_url,
            "POST",
            "/api/roles",
            {
                "code": "security_operator",
                "name": "Security Operator",
                "description": "integration test role",
            },
            token=admin_token,
        ),
        "create operator role",
    )
    role_id = int(role["id"])
    permissions = expect_ok(
        request(base_url, "GET", "/api/permissions", token=admin_token),
        "list permissions",
    )
    required_codes = {"user:update", "user:delete", "user:assign-role"}
    permission_ids = [
        int(permission["id"])
        for permission in permissions
        if permission["code"] in required_codes
    ]
    assert len(permission_ids) == len(required_codes)
    expect_ok(
        request(
            base_url,
            "PUT",
            f"/api/roles/{role_id}/permissions",
            {"permission_ids": permission_ids},
            token=admin_token,
        ),
        "assign operator permissions",
    )
    expect_ok(
        request(
            base_url,
            "PUT",
            f"/api/users/{operator_id}/roles",
            {"role_ids": [2, role_id]},
            token=admin_token,
        ),
        "assign operator role",
    )

    operator_login = expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {"email": "operator@example.com", "password": operator_password},
        ),
        "operator login",
    )
    operator_token = str(operator_login["token"])

    last_admin_update = {
        "name": "Admin",
        "email": "admin@example.com",
        "disabled": True,
    }
    expect_status(
        request(
            base_url,
            "PUT",
            f"/api/users/{admin_id}",
            last_admin_update,
            token=operator_token,
        ),
        409,
        "disable last administrator",
    )
    expect_status(
        request(
            base_url,
            "PUT",
            f"/api/users/{admin_id}/roles",
            {"role_ids": [2]},
            token=operator_token,
        ),
        409,
        "demote last administrator",
    )
    expect_status(
        request(
            base_url,
            "DELETE",
            f"/api/users/{admin_id}",
            token=operator_token,
        ),
        409,
        "delete last administrator",
    )
    expect_status(
        request(base_url, "DELETE", "/api/roles/1", token=admin_token),
        409,
        "delete built-in administrator role",
    )
    expect_status(
        request(
            base_url,
            "PUT",
            "/api/roles/1/permissions",
            {"permission_ids": []},
            token=admin_token,
        ),
        409,
        "replace built-in administrator permissions",
    )
    expect_status(
        request(
            base_url,
            "PUT",
            f"/api/users/{operator_id}/roles",
            {"role_ids": [999999]},
            token=admin_token,
        ),
        404,
        "assign nonexistent role",
    )

    expect_ok(
        request(
            base_url,
            "PUT",
            f"/api/users/{operator_id}",
            {
                "name": "Operator",
                "email": "operator@example.com",
                "password": new_operator_password,
                "disabled": False,
            },
            token=admin_token,
        ),
        "change operator password",
    )
    expect_status(
        request(base_url, "GET", "/api/users/me", token=operator_token),
        401,
        "old token after password change",
    )
    operator_login = expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {
                "email": "operator@example.com",
                "password": new_operator_password,
            },
        ),
        "operator login after password change",
    )
    operator_token = str(operator_login["token"])
    expect_status(
        request(
            base_url,
            "PUT",
            "/api/users/me/password",
            {
                "current_password": "Wrong-Current-Password!",
                "new_password": self_changed_password,
            },
            token=operator_token,
        ),
        400,
        "reject incorrect current password",
    )
    expect_ok(
        request(
            base_url,
            "PUT",
            "/api/users/me/password",
            {
                "current_password": new_operator_password,
                "new_password": self_changed_password,
            },
            token=operator_token,
        ),
        "change own password",
    )
    expect_status(
        request(base_url, "GET", "/api/users/me", token=operator_token),
        401,
        "old token after own password change",
    )
    expect_status(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {
                "email": "operator@example.com",
                "password": new_operator_password,
            },
        ),
        401,
        "old password after own password change",
    )
    operator_login = expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {
                "email": "operator@example.com",
                "password": self_changed_password,
            },
        ),
        "login with self-changed password",
    )
    operator_token = str(operator_login["token"])

    expect_ok(
        request(
            base_url,
            "PUT",
            f"/api/users/{operator_id}",
            {
                "name": "Operator",
                "email": "operator@example.com",
                "disabled": True,
            },
            token=admin_token,
        ),
        "disable operator",
    )
    expect_status(
        request(base_url, "GET", "/api/users/me", token=operator_token),
        401,
        "disabled account token",
    )
    expect_ok(
        request(
            base_url,
            "PUT",
            f"/api/users/{operator_id}",
            {
                "name": "Operator",
                "email": "operator@example.com",
                "disabled": False,
            },
            token=admin_token,
        ),
        "re-enable operator",
    )

    wrong_login = {
        "email": "operator@example.com",
        "password": "Definitely-Wrong-2026!",
    }
    for attempt in range(4):
        expect_status(
            request(base_url, "POST", "/api/auth/login", wrong_login),
            401,
            f"failed login attempt {attempt + 1}",
        )
    locked = expect_status(
        request(base_url, "POST", "/api/auth/login", wrong_login),
        429,
        "account lockout threshold",
    )
    assert int(locked[2].get("Retry-After", "0")) > 0
    expect_status(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {
                "email": "operator@example.com",
                "password": self_changed_password,
            },
        ),
        429,
        "correct password while locked",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--sql-dir", required=True)
    args = parser.parse_args()

    server = Path(args.server).resolve()
    sql_dir = Path(args.sql_dir).resolve()
    if not server.is_file():
        raise FileNotFoundError(f"server executable not found: {server}")
    if not (sql_dir / "schema.sql").is_file():
        raise FileNotFoundError(f"schema not found in: {sql_dir}")
    if not (sql_dir / "migrations").is_dir():
        raise FileNotFoundError(f"migrations not found in: {sql_dir}")

    with tempfile.TemporaryDirectory(prefix="drogon-admin-api-test-") as temp:
        root = Path(temp)
        port = free_port()
        base_url = f"http://127.0.0.1:{port}"
        write_config(root, port)
        shutil.copytree(sql_dir, root / "sql")
        prepare_legacy_database(root)

        env = os.environ.copy()
        env.update(
            {
                "DROGON_ENV": "production",
                "DROGON_JWT_SECRET": (
                    "api-integration-test-secret-with-at-least-32-characters"
                ),
                "DROGON_REGISTRATION_MODE": "first-user",
                "DROGON_CORS_ALLOWED_ORIGINS": ALLOWED_ORIGIN,
                "DROGON_ENABLE_HSTS": "true",
            }
        )
        log_path = root / "server.log"
        with log_path.open("w", encoding="utf-8") as log:
            process = subprocess.Popen(
                [str(server)],
                cwd=root,
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                wait_until_ready(base_url, process)
                run_checks(base_url)
                verify_migration_state(root)
            except Exception:
                log.flush()
                print(log_path.read_text(encoding="utf-8", errors="replace"))
                raise
            finally:
                stop_process(process)

        # Simulate an installation that already has every column from the
        # historical schema.sql but predates schema_migrations bookkeeping.
        with closing(
            sqlite3.connect(root / "data" / "drogon-admin.db")
        ) as database:
            database.execute("DELETE FROM schema_migrations")
            database.commit()

        upgrade_log_path = root / "upgrade-server.log"
        with upgrade_log_path.open("w", encoding="utf-8") as upgrade_log:
            process = subprocess.Popen(
                [str(server)],
                cwd=root,
                env=env,
                stdout=upgrade_log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                wait_until_ready(base_url, process)
                ready = expect_ok(
                    request(base_url, "GET", "/api/ready"),
                    "already-upgraded database readiness",
                )
                assert int(ready["migration_count"]) >= 1
                verify_migration_state(root)
            except Exception:
                upgrade_log.flush()
                print(
                    upgrade_log_path.read_text(
                        encoding="utf-8", errors="replace"
                    )
                )
                raise
            finally:
                stop_process(process)

    print("API integration tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
