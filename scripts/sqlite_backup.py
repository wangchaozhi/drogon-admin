#!/usr/bin/env python3
"""Create and verify transaction-consistent SQLite backups."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sqlite3
import sys
import uuid
from contextlib import closing
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.parse import quote


MANIFEST_FORMAT_VERSION = 1
DEFAULT_RETAIN = 14


class BackupError(RuntimeError):
    """An expected backup or verification failure."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def readonly_connection(path: Path, timeout: float) -> sqlite3.Connection:
    uri_path = quote(path.resolve().as_posix(), safe="/:")
    return sqlite3.connect(
        f"file:{uri_path}?mode=ro",
        uri=True,
        timeout=timeout,
    )


def validate_database(path: Path, timeout: float = 30.0) -> dict[str, Any]:
    if not path.is_file():
        raise BackupError(f"database does not exist: {path}")

    try:
        with closing(readonly_connection(path, timeout)) as database:
            integrity = [
                str(row[0])
                for row in database.execute("PRAGMA integrity_check")
            ]
            if integrity != ["ok"]:
                raise BackupError(
                    "SQLite integrity_check failed: " + "; ".join(integrity[:10])
                )

            foreign_key_errors = list(
                database.execute("PRAGMA foreign_key_check")
            )
            if foreign_key_errors:
                sample = "; ".join(
                    ",".join(str(value) for value in row)
                    for row in foreign_key_errors[:10]
                )
                raise BackupError(f"SQLite foreign_key_check failed: {sample}")

            migrations: list[dict[str, Any]] = []
            has_migrations = database.execute(
                "SELECT 1 FROM sqlite_master "
                "WHERE type='table' AND name='schema_migrations'"
            ).fetchone()
            if has_migrations:
                columns = {
                    str(row[1])
                    for row in database.execute(
                        "PRAGMA table_info(schema_migrations)"
                    )
                }
                if {"version", "applied_at"}.issubset(columns):
                    checksum_expression = (
                        "COALESCE(checksum,'')" if "checksum" in columns else "''"
                    )
                    rows = database.execute(
                        "SELECT version,"
                        f"{checksum_expression} AS checksum,"
                        "applied_at FROM schema_migrations ORDER BY version"
                    )
                    migrations = [
                        {
                            "version": str(row[0]),
                            "checksum": str(row[1]),
                            "applied_at": int(row[2]),
                        }
                        for row in rows
                    ]

            return {
                "sqlite_version": sqlite3.sqlite_version,
                "schema_migrations": migrations,
            }
    except sqlite3.Error as error:
        raise BackupError(f"cannot validate SQLite database: {error}") from error


def manifest_path(database_path: Path) -> Path:
    return database_path.with_name(database_path.name + ".manifest.json")


def prune_backups(output_dir: Path, prefix: str, retain: int) -> list[str]:
    if retain <= 0:
        raise BackupError("retain must be greater than zero")

    manifests = sorted(
        output_dir.glob(f"{prefix}-*.db.manifest.json"),
        key=lambda path: path.name,
    )
    removed: list[str] = []
    for old_manifest in manifests[:-retain]:
        try:
            metadata = json.loads(old_manifest.read_text(encoding="utf-8"))
            database_name = metadata.get("database")
            if not isinstance(database_name, str):
                continue
            old_database = output_dir / database_name
            if old_database.parent != output_dir or not old_database.name.startswith(
                f"{prefix}-"
            ):
                continue
            old_database.unlink(missing_ok=True)
            old_manifest.unlink()
            removed.append(database_name)
        except (OSError, json.JSONDecodeError):
            # Never delete data based on an unreadable or malformed manifest.
            continue
    return removed


def create_backup(
    source: Path,
    output_dir: Path,
    retain: int = DEFAULT_RETAIN,
    timeout: float = 30.0,
) -> dict[str, Any]:
    source = source.resolve()
    output_dir = output_dir.resolve()
    if not source.is_file():
        raise BackupError(f"source database does not exist: {source}")
    if retain <= 0:
        raise BackupError("retain must be greater than zero")
    if timeout <= 0:
        raise BackupError("timeout must be greater than zero")

    output_dir.mkdir(parents=True, exist_ok=True)
    if not output_dir.is_dir():
        raise BackupError(f"backup output is not a directory: {output_dir}")

    safe_prefix = re.sub(r"[^A-Za-z0-9_.-]+", "-", source.stem).strip(".-")
    safe_prefix = safe_prefix or "sqlite"
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    backup_path = output_dir / f"{safe_prefix}-{timestamp}.db"
    if backup_path == source:
        raise BackupError("backup path must differ from the source database")
    temporary = output_dir / f".{backup_path.name}.{uuid.uuid4().hex}.tmp"

    try:
        with closing(readonly_connection(source, timeout)) as source_database:
            with closing(
                sqlite3.connect(temporary, timeout=timeout)
            ) as destination:
                source_database.backup(destination, pages=256, sleep=0.05)

        validation = validate_database(temporary, timeout)
        with temporary.open("rb+") as stream:
            os.fsync(stream.fileno())
        os.chmod(temporary, 0o600)
        checksum = sha256_file(temporary)
        size_bytes = temporary.stat().st_size

        metadata: dict[str, Any] = {
            "format_version": MANIFEST_FORMAT_VERSION,
            "created_at": datetime.now(timezone.utc).isoformat(),
            "source_database": source.name,
            "database": backup_path.name,
            "size_bytes": size_bytes,
            "sha256": checksum,
            **validation,
        }
        manifest = manifest_path(backup_path)
        manifest_temporary = manifest.with_name(
            f".{manifest.name}.{uuid.uuid4().hex}.tmp"
        )
        try:
            with manifest_temporary.open(
                "x", encoding="utf-8", newline="\n"
            ) as stream:
                json.dump(
                    metadata,
                    stream,
                    ensure_ascii=False,
                    indent=2,
                    sort_keys=True,
                )
                stream.write("\n")
                stream.flush()
                os.fsync(stream.fileno())
            os.chmod(manifest_temporary, 0o600)
            os.replace(temporary, backup_path)
            try:
                os.replace(manifest_temporary, manifest)
            except OSError:
                try:
                    backup_path.unlink(missing_ok=True)
                except OSError:
                    pass
                raise
        finally:
            manifest_temporary.unlink(missing_ok=True)

        metadata["removed_backups"] = prune_backups(
            output_dir, safe_prefix, retain
        )
        return metadata
    except sqlite3.Error as error:
        raise BackupError(f"SQLite online backup failed: {error}") from error
    finally:
        temporary.unlink(missing_ok=True)


def verify_backup(
    database_path: Path,
    supplied_manifest: Path | None = None,
    timeout: float = 30.0,
) -> dict[str, Any]:
    database_path = database_path.resolve()
    if timeout <= 0:
        raise BackupError("timeout must be greater than zero")
    expected_manifest = (
        supplied_manifest.resolve()
        if supplied_manifest is not None
        else manifest_path(database_path)
    )
    if not expected_manifest.is_file():
        raise BackupError(f"backup manifest does not exist: {expected_manifest}")

    try:
        metadata = json.loads(expected_manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise BackupError(f"cannot read backup manifest: {error}") from error
    if not isinstance(metadata, dict):
        raise BackupError("backup manifest must contain a JSON object")
    if metadata.get("format_version") != MANIFEST_FORMAT_VERSION:
        raise BackupError("unsupported backup manifest format")
    if metadata.get("database") != database_path.name:
        raise BackupError("backup manifest database name does not match")

    expected_size = metadata.get("size_bytes")
    if not isinstance(expected_size, int) or expected_size < 0:
        raise BackupError("backup manifest has an invalid size")
    if not database_path.is_file():
        raise BackupError(f"backup database does not exist: {database_path}")
    if database_path.stat().st_size != expected_size:
        raise BackupError("backup size does not match manifest")

    expected_checksum = metadata.get("sha256")
    if not isinstance(expected_checksum, str) or not re.fullmatch(
        r"[0-9a-f]{64}", expected_checksum
    ):
        raise BackupError("backup manifest has an invalid SHA-256")
    actual_checksum = sha256_file(database_path)
    if actual_checksum != expected_checksum:
        raise BackupError("backup SHA-256 does not match manifest")

    validation = validate_database(database_path, timeout)
    return {
        "database": database_path.name,
        "size_bytes": expected_size,
        "sha256": actual_checksum,
        **validation,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Create a transaction-consistent SQLite backup using the online "
            "backup API, or verify an existing backup and manifest."
        )
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    backup = subparsers.add_parser("backup", help="create and verify a backup")
    backup.add_argument("--source", required=True, type=Path)
    backup.add_argument("--output-dir", required=True, type=Path)
    backup.add_argument("--retain", type=int, default=DEFAULT_RETAIN)
    backup.add_argument("--timeout", type=float, default=30.0)

    verify = subparsers.add_parser("verify", help="verify a backup and manifest")
    verify.add_argument("--database", required=True, type=Path)
    verify.add_argument("--manifest", type=Path)
    verify.add_argument("--timeout", type=float, default=30.0)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.command == "backup":
            result = create_backup(
                args.source,
                args.output_dir,
                args.retain,
                args.timeout,
            )
        else:
            result = verify_backup(
                args.database,
                args.manifest,
                args.timeout,
            )
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
        return 0
    except (BackupError, OSError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
