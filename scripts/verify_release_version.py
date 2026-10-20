#!/usr/bin/env python3
"""Verify that every release version source and the optional tag agree."""

from __future__ import annotations

import argparse
import json
import os
import re
from pathlib import Path


SEMVER = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
    r"(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?"
    r"(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$"
)


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as stream:
        return json.load(stream)


def expected_tag(cli_tag: str | None) -> str | None:
    if cli_tag:
        return cli_tag
    workflow_tag = os.getenv("EXPECTED_RELEASE_TAG", "").strip()
    if workflow_tag:
        return workflow_tag
    if os.getenv("GITHUB_REF_TYPE") == "tag":
        return os.getenv("GITHUB_REF_NAME", "").strip() or None
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--tag",
        help="optional release tag; defaults to GitHub Actions environment",
    )
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    canonical = (root / "VERSION").read_text(encoding="utf-8").strip()
    errors: list[str] = []
    if not SEMVER.fullmatch(canonical):
        errors.append(f"VERSION is not valid SemVer: {canonical!r}")

    vcpkg_version = str(load_json(root / "vcpkg.json").get("version", ""))
    package = load_json(root / "front" / "package.json")
    package_version = str(package.get("version", ""))
    package_lock = load_json(root / "front" / "package-lock.json")
    lock_version = str(package_lock.get("version", ""))
    lock_root_version = str(
        package_lock.get("packages", {}).get("", {}).get("version", "")
    )

    cmake_text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    cmake_match = re.search(
        r"project\s*\(\s*drogon-admin\s+VERSION\s+([^\s\)]+)",
        cmake_text,
        re.IGNORECASE,
    )
    cmake_version = cmake_match.group(1) if cmake_match else ""

    sources = {
        "vcpkg.json": vcpkg_version,
        "front/package.json": package_version,
        "front/package-lock.json": lock_version,
        "front/package-lock.json packages['']": lock_root_version,
    }
    for source, version in sources.items():
        if version != canonical:
            errors.append(
                f"{source} version {version!r} does not match "
                f"VERSION {canonical!r}"
            )

    cmake_expected = re.split(r"[-+]", canonical, maxsplit=1)[0]
    if cmake_version != cmake_expected:
        errors.append(
            f"CMakeLists.txt version {cmake_version!r} does not match "
            f"VERSION core {cmake_expected!r}"
        )

    tag = expected_tag(args.tag)
    if tag:
        if not tag.startswith("v"):
            errors.append(f"release tag must start with 'v': {tag!r}")
        elif tag[1:] != canonical:
            errors.append(
                f"release tag {tag!r} does not match expected "
                f"'v{canonical}'"
            )

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    suffix = f", tag={tag}" if tag else ""
    print(f"Release version verified: {canonical}{suffix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
