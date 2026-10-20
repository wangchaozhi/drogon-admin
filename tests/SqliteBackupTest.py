#!/usr/bin/env python3
"""Regression tests for the SQLite disaster-recovery backup tool."""

from __future__ import annotations

import json
import sqlite3
import sys
import tempfile
import unittest
from contextlib import closing
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from scripts.sqlite_backup import (  # noqa: E402
    BackupError,
    create_backup,
    manifest_path,
    verify_backup,
)


def create_wal_database(path: Path) -> sqlite3.Connection:
    database = sqlite3.connect(path)
    database.execute("PRAGMA journal_mode=WAL")
    database.execute("PRAGMA wal_autocheckpoint=0")
    database.execute("PRAGMA foreign_keys=ON")
    database.executescript(
        """
        CREATE TABLE parent (
            id INTEGER PRIMARY KEY,
            value TEXT NOT NULL
        );
        CREATE TABLE child (
            id INTEGER PRIMARY KEY,
            parent_id INTEGER NOT NULL REFERENCES parent(id)
        );
        CREATE TABLE schema_migrations (
            version TEXT PRIMARY KEY,
            checksum TEXT NOT NULL,
            applied_at INTEGER NOT NULL
        );
        INSERT INTO parent(id,value) VALUES(1,'from-wal');
        INSERT INTO child(id,parent_id) VALUES(1,1);
        INSERT INTO schema_migrations(version,checksum,applied_at)
            VALUES('001_test','aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',1700000000);
        """
    )
    database.commit()
    return database


class SqliteBackupTest(unittest.TestCase):
    def test_online_backup_includes_wal_and_verifies_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "live.db"
            output = root / "backups"
            writer = create_wal_database(source)
            try:
                result = create_backup(source, output, retain=3)
            finally:
                writer.close()

            backup = output / str(result["database"])
            manifest = manifest_path(backup)
            self.assertTrue(backup.is_file())
            self.assertTrue(manifest.is_file())
            self.assertEqual(result["source_database"], "live.db")
            self.assertEqual(
                result["schema_migrations"][0]["version"],
                "001_test",
            )

            with closing(sqlite3.connect(backup)) as copied:
                self.assertEqual(
                    copied.execute(
                        "SELECT value FROM parent WHERE id=1"
                    ).fetchone(),
                    ("from-wal",),
                )
                self.assertEqual(
                    copied.execute("PRAGMA journal_mode").fetchone()[0],
                    "wal",
                )

            verified = verify_backup(backup)
            self.assertEqual(verified["sha256"], result["sha256"])
            self.assertEqual(verified["size_bytes"], result["size_bytes"])

    def test_retention_only_keeps_requested_successful_backups(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "live.db"
            output = root / "backups"
            writer = create_wal_database(source)
            try:
                results = [
                    create_backup(source, output, retain=2)
                    for _ in range(4)
                ]
            finally:
                writer.close()

            backups = sorted(output.glob("live-*.db"))
            manifests = sorted(output.glob("live-*.db.manifest.json"))
            self.assertEqual(len(backups), 2)
            self.assertEqual(len(manifests), 2)
            self.assertFalse((output / results[0]["database"]).exists())
            self.assertTrue((output / results[-1]["database"]).exists())

    def test_tampered_backup_or_manifest_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "live.db"
            output = root / "backups"
            writer = create_wal_database(source)
            try:
                result = create_backup(source, output)
            finally:
                writer.close()

            backup = output / str(result["database"])
            manifest = manifest_path(backup)
            original_backup = backup.read_bytes()
            backup.write_bytes(original_backup + b"tampered")
            with self.assertRaisesRegex(BackupError, "size"):
                verify_backup(backup)
            backup.write_bytes(original_backup)

            metadata = json.loads(manifest.read_text(encoding="utf-8"))
            metadata["sha256"] = "0" * 64
            manifest.write_text(json.dumps(metadata), encoding="utf-8")
            with self.assertRaisesRegex(BackupError, "SHA-256"):
                verify_backup(backup)

    def test_foreign_key_violation_never_publishes_backup(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "invalid.db"
            output = root / "backups"
            with closing(sqlite3.connect(source)) as database:
                database.executescript(
                    """
                    PRAGMA foreign_keys=OFF;
                    CREATE TABLE parent(id INTEGER PRIMARY KEY);
                    CREATE TABLE child(
                        id INTEGER PRIMARY KEY,
                        parent_id INTEGER REFERENCES parent(id)
                    );
                    INSERT INTO child(id,parent_id) VALUES(1,999);
                    """
                )

            with self.assertRaisesRegex(BackupError, "foreign_key_check"):
                create_backup(source, output)
            self.assertEqual(list(output.glob("*.db")), [])
            self.assertEqual(list(output.glob("*.manifest.json")), [])

    def test_missing_manifest_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            database = Path(temp) / "orphan.db"
            with closing(sqlite3.connect(database)) as connection:
                connection.execute("CREATE TABLE example(id INTEGER)")
            with self.assertRaisesRegex(BackupError, "manifest"):
                verify_backup(database)


if __name__ == "__main__":
    unittest.main()
