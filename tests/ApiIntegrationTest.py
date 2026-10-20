#!/usr/bin/env python3
"""Black-box authentication, RBAC and CORS regression test."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
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
QUERY_SECRET = "diagnostic-query-secret-2026"
JWT_SECRET = "api-integration-test-secret-with-at-least-32-characters"


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
                "log_level": "INFO",
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
            "login_rate_limit_global": 1000,
            "login_rate_limit_per_identifier": 15,
            "login_rate_limit_window_seconds": 60,
            "cors_allowed_origins": [ALLOWED_ORIGIN],
            "cors_allow_credentials": False,
            "cors_max_age_seconds": 600,
            "enable_hsts": True,
            "shutdown_grace_seconds": 10,
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


def prepare_legacy_migration_metadata(root: Path) -> None:
    """Remove checksum metadata to emulate databases from older releases."""
    with closing(
        sqlite3.connect(root / "data" / "drogon-admin.db")
    ) as database:
        database.executescript(
            """
            ALTER TABLE schema_migrations
                RENAME TO schema_migrations_with_checksum;
            CREATE TABLE schema_migrations (
                version TEXT PRIMARY KEY,
                applied_at INTEGER NOT NULL
            );
            INSERT INTO schema_migrations(version,applied_at)
                SELECT version,applied_at
                FROM schema_migrations_with_checksum;
            DROP TABLE schema_migrations_with_checksum;
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
        expected_versions = {
            "001_user_auth_security",
            "002_email_nocase",
            "003_rbac_foreign_keys",
            "004_audit_logs",
        }
        if not expected_versions.issubset(versions):
            raise AssertionError(
                "migration versions were not recorded: "
                f"{sorted(expected_versions - versions)}"
            )
        migration_columns = {
            row[1]
            for row in database.execute(
                "PRAGMA table_info(schema_migrations)"
            )
        }
        if "checksum" not in migration_columns:
            raise AssertionError("migration checksum column is missing")
        checksums = dict(
            database.execute(
                "SELECT version,checksum FROM schema_migrations"
            )
        )
        for version in expected_versions:
            migration_path = root / "sql" / "migrations" / f"{version}.sql"
            migration_contents = migration_path.read_bytes()
            migration_contents = migration_contents.replace(
                b"\r\n", b"\n"
            ).replace(b"\r", b"\n")
            expected_checksum = hashlib.sha256(
                migration_contents
            ).hexdigest()
            if checksums.get(version) != expected_checksum:
                raise AssertionError(
                    f"migration checksum is invalid: {version}"
                )
        audit_columns = {
            row[1] for row in database.execute(
                "PRAGMA table_info(audit_logs)"
            )
        }
        expected_audit_columns = {
            "id",
            "actor_user_id",
            "action",
            "target_type",
            "target_id",
            "request_id",
            "details",
            "created_at",
        }
        if not expected_audit_columns.issubset(audit_columns):
            raise AssertionError("audit_logs schema is incomplete")
        audit_indexes = {
            row[1] for row in database.execute(
                "PRAGMA index_list(audit_logs)"
            )
        }
        if "idx_audit_logs_action_created" not in audit_indexes:
            raise AssertionError("audit action index is missing")


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


def wait_for_startup_failure(
    process: subprocess.Popen[Any],
    log_path: Path,
    expected_message: str,
) -> None:
    try:
        return_code = process.wait(timeout=10)
    except subprocess.TimeoutExpired as error:
        raise AssertionError(
            "server did not reject invalid migration state"
        ) from error
    if return_code == 0:
        raise AssertionError("server unexpectedly accepted invalid migration state")
    log = log_path.read_text(encoding="utf-8", errors="replace")
    if expected_message not in log:
        raise AssertionError(
            f"startup failure did not contain {expected_message!r}:\n{log}"
        )


def run_checks(base_url: str) -> tuple[set[str], int]:
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
    assert health[2].get("Cache-Control") == "no-store"
    assert "camera=()" in health[2].get("Permissions-Policy", "")
    assert health[1]["data"]["lifecycle"] == "ready"

    ready = expect_ok(
        request(base_url, "GET", "/api/ready"),
        "database readiness",
    )
    assert ready["status"] == "ready"
    assert ready["database"] == "ok"
    assert ready["journal_mode"] == "wal"
    assert ready["foreign_keys"] is True
    assert int(ready["busy_timeout_ms"]) >= 5000
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
    assert "drogon_admin_login_rate_limited_total" in metrics[1]
    assert "drogon_admin_service_ready 1" in metrics[1]
    assert "drogon_admin_service_draining 0" in metrics[1]
    assert metrics[2].get_content_type() == "text/plain"
    assert metrics[2].get("Cache-Control") == "no-store"

    not_found = expect_status(
        request(
            base_url,
            "GET",
            f"/api/not-found?access_token={QUERY_SECRET}",
            headers={"X-Request-ID": "error-request-123"},
        ),
        404,
        "API 404 security headers",
    )
    assert not_found[2].get("X-Frame-Options") == "DENY"
    assert not_found[2].get("Cache-Control") == "no-store"
    assert not_found[2].get("X-Request-ID") == "error-request-123"

    invalid_login = expect_status(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {},
            headers={"X-Request-ID": "short"},
        ),
        400,
        "invalid login",
    )
    assert invalid_login[2].get("Cache-Control") == "no-store"
    generated_request_id = invalid_login[2].get("X-Request-ID", "")
    assert generated_request_id != "short"
    assert re.fullmatch(r"[A-Za-z0-9._-]{8,128}", generated_request_id)
    expect_status(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {"email": 42, "password": "Wrong-Type-2026!"},
        ),
        400,
        "reject non-string login fields",
    )
    unknown_login = expect_status(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {
                "email": "unknown@example.com",
                "password": "Unknown-Horse-2026!",
            },
        ),
        401,
        "unknown account login",
    )
    assert unknown_login[1]["message"] == "email or password incorrect"

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
                "email": " Admin@Example.COM ",
                "password": admin_password,
            },
        ),
        "first administrator registration",
    )
    admin_id = int(admin["id"])
    assert admin["email"] == "admin@example.com"
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
            {"email": " ADMIN@example.COM ", "password": admin_password},
        ),
        "administrator login",
    )
    admin_token = str(admin_login["token"])
    assert "audit:view" in admin_login["permissions"]
    assert any(
        child.get("path") == "/system/audit-logs"
        for menu in admin_login["menus"]
        for child in menu.get("children", [])
    )
    me = expect_ok(
        request(base_url, "GET", "/api/users/me", token=admin_token),
        "authenticated profile",
    )
    assert int(me["user"]["id"]) == admin_id
    expect_status(
        request(
            base_url,
            "POST",
            "/api/users",
            {
                "name": "Duplicate Admin",
                "email": "ADMIN@EXAMPLE.COM",
                "password": "Duplicate-Horse-2026!",
            },
            token=admin_token,
        ),
        409,
        "reject case-variant duplicate email",
    )
    expect_status(
        request(
            base_url,
            "POST",
            "/api/menus",
            {
                "parent_id": 999999,
                "name": "Orphan",
                "type": "menu",
            },
            token=admin_token,
        ),
        404,
        "reject nonexistent menu parent",
    )
    parent_menu = expect_ok(
        request(
            base_url,
            "POST",
            "/api/menus",
            {
                "parent_id": 0,
                "name": "Operations",
                "type": "dir",
            },
            token=admin_token,
        ),
        "create parent menu",
    )
    parent_menu_id = int(parent_menu["id"])
    child_menu = expect_ok(
        request(
            base_url,
            "POST",
            "/api/menus",
            {
                "parent_id": parent_menu_id,
                "name": "Jobs",
                "type": "menu",
            },
            token=admin_token,
        ),
        "create child menu",
    )
    child_menu_id = int(child_menu["id"])
    expect_status(
        request(
            base_url,
            "PUT",
            f"/api/menus/{parent_menu_id}",
            {
                "parent_id": parent_menu_id,
                "name": "Operations",
                "type": "dir",
            },
            token=admin_token,
        ),
        400,
        "reject self-parent menu",
    )
    expect_status(
        request(
            base_url,
            "PUT",
            f"/api/menus/{parent_menu_id}",
            {
                "parent_id": child_menu_id,
                "name": "Operations",
                "type": "dir",
            },
            token=admin_token,
        ),
        400,
        "reject cyclic menu hierarchy",
    )
    expect_ok(
        request(
            base_url,
            "PUT",
            f"/api/menus/{child_menu_id}",
            {
                "parent_id": parent_menu_id,
                "name": "Scheduled Jobs",
                "path": "/operations/jobs",
                "type": "menu",
            },
            token=admin_token,
        ),
        "update child menu",
    )
    expect_status(
        request(
            base_url,
            "DELETE",
            f"/api/menus/{parent_menu_id}",
            token=admin_token,
        ),
        409,
        "reject deleting menu with children",
    )
    expect_ok(
        request(
            base_url,
            "DELETE",
            f"/api/menus/{child_menu_id}",
            token=admin_token,
        ),
        "delete child menu",
    )
    expect_ok(
        request(
            base_url,
            "DELETE",
            f"/api/menus/{parent_menu_id}",
            token=admin_token,
        ),
        "delete empty parent menu",
    )
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
            headers={"X-Request-ID": "audit-create-operator-123"},
        ),
        "create operator",
    )
    operator_id = int(operator["id"])

    disposable = expect_ok(
        request(
            base_url,
            "POST",
            "/api/users",
            {
                "name": "Disposable",
                "email": "disposable@example.com",
                "password": "Disposable-Horse-2026!",
            },
            token=admin_token,
        ),
        "create disposable user",
    )
    expect_ok(
        request(
            base_url,
            "DELETE",
            f"/api/users/{int(disposable['id'])}",
            token=admin_token,
        ),
        "delete disposable user",
    )

    expect_status(
        request(
            base_url,
            "POST",
            "/api/roles",
            {
                "code": "Invalid Role Code",
                "name": "Invalid",
            },
            token=admin_token,
        ),
        400,
        "reject invalid role code",
    )
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
    expect_ok(
        request(
            base_url,
            "PUT",
            f"/api/roles/{role_id}",
            {
                "name": "Security Operator Updated",
                "description": "updated integration test role",
            },
            token=admin_token,
        ),
        "update operator role",
    )
    expect_status(
        request(
            base_url,
            "PUT",
            f"/api/roles/{role_id}/permissions",
            {"permission_ids": [1.5]},
            token=admin_token,
        ),
        400,
        "reject non-integer permission ids",
    )
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
    assert "audit:view" not in operator_login["permissions"]
    assert not any(
        child.get("path") == "/system/audit-logs"
        for menu in operator_login["menus"]
        for child in menu.get("children", [])
    )
    expect_status(
        request(
            base_url,
            "GET",
            "/api/audit-logs",
            token=operator_token,
        ),
        403,
        "deny audit logs without audit:view",
    )
    expect_ok(
        request(
            base_url,
            "POST",
            f"/api/users/{operator_id}/revoke-sessions",
            token=admin_token,
        ),
        "administrator revokes user sessions",
    )
    expect_status(
        request(base_url, "GET", "/api/users/me", token=operator_token),
        401,
        "operator token after administrator revocation",
    )
    operator_login = expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {
                "email": "operator@example.com",
                "password": operator_password,
            },
        ),
        "operator login after session revocation",
    )
    operator_token = str(operator_login["token"])
    expect_status(
        request(
            base_url,
            "POST",
            f"/api/users/{admin_id}/revoke-sessions",
            token=admin_token,
        ),
        400,
        "reject self session revocation through administrator endpoint",
    )

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
    locked_users = expect_ok(
        request(
            base_url,
            "GET",
            "/api/users?page=1&page_size=200",
            token=admin_token,
        ),
        "list users with login lock state",
    )
    locked_operator = next(
        user
        for user in locked_users["items"]
        if int(user["id"]) == operator_id
    )
    assert int(locked_operator["locked_until"]) > int(time.time())
    expect_ok(
        request(
            base_url,
            "POST",
            f"/api/users/{operator_id}/unlock",
            token=admin_token,
        ),
        "administrator unlocks operator login",
    )
    unlocked_login = expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            {
                "email": "operator@example.com",
                "password": self_changed_password,
            },
        ),
        "operator login after administrator unlock",
    )
    operator_token = str(unlocked_login["token"])
    expect_ok(
        request(
            base_url,
            "DELETE",
            f"/api/roles/{role_id}",
            token=admin_token,
        ),
        "delete custom role with foreign keys enabled",
    )

    expect_status(
        request(
            base_url,
            "GET",
            "/api/audit-logs?page_size=201",
            token=admin_token,
        ),
        400,
        "reject excessive audit page size",
    )
    audit_page = expect_ok(
        request(
            base_url,
            "GET",
            "/api/audit-logs?page=1&page_size=200",
            token=admin_token,
        ),
        "list audit logs",
    )
    assert int(audit_page["total"]) == len(audit_page["items"])
    actions = {entry["action"] for entry in audit_page["items"]}
    expected_actions = {
        "user.create",
        "user.update",
        "user.delete",
        "user.assign_roles",
        "user.revoke_sessions",
        "user.unlock",
        "user.change_password",
        "role.create",
        "role.update",
        "role.delete",
        "role.assign_permissions",
        "menu.create",
        "menu.update",
        "menu.delete",
    }
    assert expected_actions.issubset(actions)
    operator_create = next(
        entry
        for entry in audit_page["items"]
        if entry["action"] == "user.create"
        and int(entry["target_id"]) == operator_id
    )
    assert operator_create["actor_user_id"] == admin_id
    assert operator_create["request_id"] == "audit-create-operator-123"

    serialized_details = json.dumps(
        [entry["details"] for entry in audit_page["items"]],
        ensure_ascii=False,
    )
    for secret in {
        admin_password,
        operator_password,
        new_operator_password,
        self_changed_password,
        "Disposable-Horse-2026!",
        admin_token,
        operator_token,
    }:
        assert secret not in serialized_details

    role_delete_logs = expect_ok(
        request(
            base_url,
            "GET",
            "/api/audit-logs?action=role.delete",
            token=admin_token,
        ),
        "filter audit logs by action",
    )
    assert role_delete_logs["items"]
    assert all(
        entry["action"] == "role.delete"
        for entry in role_delete_logs["items"]
    )
    expect_ok(
        request(
            base_url,
            "POST",
            "/api/auth/logout",
            token=admin_token,
        ),
        "server-side logout",
    )
    expect_status(
        request(base_url, "GET", "/api/users/me", token=admin_token),
        401,
        "token after server-side logout",
    )

    rate_limited_login = {
        "email": "rate-limit-probe@example.com",
        "password": "Unknown-Horse-2026!",
    }
    for attempt in range(15):
        expect_status(
            request(
                base_url,
                "POST",
                "/api/auth/login",
                rate_limited_login,
            ),
            401,
            f"login rate-limit budget attempt {attempt + 1}",
        )
    throttled = expect_status(
        request(
            base_url,
            "POST",
            "/api/auth/login",
            rate_limited_login,
        ),
        429,
        "login identifier rate limit",
    )
    assert throttled[1]["code"] == 4292
    assert int(throttled[2].get("Retry-After", "0")) > 0

    metrics_after_throttle = expect_status(
        request(base_url, "GET", "/api/metrics"),
        200,
        "login rate-limit metrics",
    )
    assert (
        'drogon_admin_login_rate_limited_total{scope="identifier"} 1'
        in metrics_after_throttle[1]
    )
    return (
        {
            QUERY_SECRET,
            admin_password,
            operator_password,
            new_operator_password,
            self_changed_password,
            "Disposable-Horse-2026!",
            admin_token,
            operator_token,
        },
        operator_id,
    )


def verify_request_diagnostics(
    log_path: Path,
    secrets: set[str],
    dynamic_user_id: int,
) -> None:
    log = log_path.read_text(encoding="utf-8", errors="replace")
    for secret in secrets:
        if secret in log:
            raise AssertionError("sensitive request value was written to logs")

    decoder = json.JSONDecoder()
    access_entries: list[dict[str, Any]] = []
    for line in log.splitlines():
        if '"event":"http_access"' not in line:
            continue
        start = line.find("{")
        if start < 0:
            continue
        value, _ = decoder.raw_decode(line[start:])
        if isinstance(value, dict):
            access_entries.append(value)

    if not access_entries:
        raise AssertionError("structured access logs were not emitted")
    required_fields = {
        "event",
        "request_id",
        "method",
        "path_template",
        "status",
        "duration_us",
    }
    if any(not required_fields.issubset(entry) for entry in access_entries):
        raise AssertionError("structured access log fields are incomplete")

    error_entry = next(
        (
            entry
            for entry in access_entries
            if entry.get("request_id") == "error-request-123"
        ),
        None,
    )
    if error_entry is None:
        raise AssertionError("error access log lost its request ID")
    assert error_entry["status"] == 404
    assert error_entry["path_template"] == "<unmatched>"

    dynamic_entries = [
        entry
        for entry in access_entries
        if str(entry.get("path_template", "")).startswith("/api/users/{id:")
    ]
    if not dynamic_entries:
        raise AssertionError("dynamic route template was not logged")
    if any(
        str(dynamic_user_id) in str(entry["path_template"])
        for entry in dynamic_entries
    ):
        raise AssertionError("raw dynamic path was written to access logs")


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
        secret_path = root / "secrets" / "jwt-secret"
        secret_path.parent.mkdir()
        secret_path.write_text(JWT_SECRET + "\n", encoding="utf-8")

        env = os.environ.copy()
        env.update(
            {
                "DROGON_ENV": "production",
                "DROGON_JWT_SECRET_FILE": str(secret_path),
                "DROGON_REGISTRATION_MODE": "first-user",
                "DROGON_CORS_ALLOWED_ORIGINS": ALLOWED_ORIGIN,
                "DROGON_ENABLE_HSTS": "true",
            }
        )
        env.pop("DROGON_JWT_SECRET", None)
        log_path = root / "server.log"
        diagnostic_secrets: set[str] = set()
        dynamic_user_id = 0
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
                diagnostic_secrets, dynamic_user_id = run_checks(base_url)
                verify_migration_state(root)
            except Exception:
                log.flush()
                print(log_path.read_text(encoding="utf-8", errors="replace"))
                raise
            finally:
                stop_process(process)

        diagnostic_secrets.add(JWT_SECRET)
        verify_request_diagnostics(
            log_path,
            diagnostic_secrets,
            dynamic_user_id,
        )

        if os.name != "nt":
            shutdown_log = log_path.read_text(
                encoding="utf-8", errors="replace"
            )
            assert "graceful shutdown started signal=SIGTERM" in shutdown_log
            assert "graceful shutdown drained all requests" in shutdown_log
            assert "drogon-admin stopped" in shutdown_log

        conflicting_env = env.copy()
        conflicting_env["DROGON_JWT_SECRET"] = JWT_SECRET
        conflict_log_path = root / "secret-source-conflict.log"
        with conflict_log_path.open("w", encoding="utf-8") as conflict_log:
            process = subprocess.Popen(
                [str(server)],
                cwd=root,
                env=conflicting_env,
                stdout=conflict_log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                wait_for_startup_failure(
                    process,
                    conflict_log_path,
                    "set only one of DROGON_JWT_SECRET and "
                    "DROGON_JWT_SECRET_FILE",
                )
            finally:
                stop_process(process)

        secret_path.write_text(
            JWT_SECRET + "\nunexpected-second-line\n",
            encoding="utf-8",
        )
        multiline_log_path = root / "multiline-secret.log"
        with multiline_log_path.open("w", encoding="utf-8") as multiline_log:
            process = subprocess.Popen(
                [str(server)],
                cwd=root,
                env=env,
                stdout=multiline_log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                wait_for_startup_failure(
                    process,
                    multiline_log_path,
                    "DROGON_JWT_SECRET_FILE must contain exactly one text line",
                )
            finally:
                stop_process(process)
        secret_path.write_text(JWT_SECRET + "\n", encoding="utf-8")

        # Existing installations have schema_migrations without a checksum
        # column. The upgrade must add it and establish a checksum baseline.
        prepare_legacy_migration_metadata(root)
        checksum_upgrade_log_path = root / "checksum-upgrade-server.log"
        with checksum_upgrade_log_path.open(
            "w", encoding="utf-8"
        ) as checksum_upgrade_log:
            process = subprocess.Popen(
                [str(server)],
                cwd=root,
                env=env,
                stdout=checksum_upgrade_log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                wait_until_ready(base_url, process)
                verify_migration_state(root)
            except Exception:
                checksum_upgrade_log.flush()
                print(
                    checksum_upgrade_log_path.read_text(
                        encoding="utf-8", errors="replace"
                    )
                )
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

        # Applied migrations are immutable: changing even a comment must make
        # startup fail before the service begins accepting requests.
        migration_path = (
            root / "sql" / "migrations" / "001_user_auth_security.sql"
        )
        migration_path.write_bytes(
            migration_path.read_bytes()
            + b"\n-- integration-test checksum mismatch\n"
        )
        mismatch_log_path = root / "checksum-mismatch-server.log"
        with mismatch_log_path.open(
            "w", encoding="utf-8"
        ) as mismatch_log:
            process = subprocess.Popen(
                [str(server)],
                cwd=root,
                env=env,
                stdout=mismatch_log,
                stderr=subprocess.STDOUT,
                text=True,
            )
            try:
                wait_for_startup_failure(
                    process,
                    mismatch_log_path,
                    "migration checksum mismatch: 001_user_auth_security",
                )
            finally:
                stop_process(process)

    print("API integration tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
