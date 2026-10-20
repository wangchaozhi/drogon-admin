#!/usr/bin/env python3
"""Validate the release workflow orchestration policy without PyYAML."""

from __future__ import annotations

from pathlib import Path


COMPONENT_WORKFLOWS = (
    "backend.yml",
    "frontend.yml",
    "musl-release.yml",
)


def require(text: str, needle: str, source: str, errors: list[str]) -> None:
    if needle not in text:
        errors.append(f"{source}: missing required policy: {needle!r}")


def forbid(text: str, needle: str, source: str, errors: list[str]) -> None:
    if needle in text:
        errors.append(f"{source}: forbidden release policy: {needle!r}")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    workflows = root / ".github" / "workflows"
    errors: list[str] = []

    release_path = workflows / "release.yml"
    release = release_path.read_text(encoding="utf-8")
    release_source = str(release_path.relative_to(root))
    for policy in (
        "workflow_dispatch:",
        "actions: write",
        "contents: write",
        "cancel-in-progress: false",
        "redispatch_existing:",
        'if [[ "$GITHUB_REF" != "refs/heads/main" ]]',
        "EXPECTED_RELEASE_TAG: ${{ inputs.tag }}",
        'if [[ "$TAG_COMMIT" != "$GITHUB_SHA" ]]',
        'git tag -a "$TAG"',
        'git push origin "refs/tags/$TAG"',
    ):
        require(release, policy, release_source, errors)

    for workflow in COMPONENT_WORKFLOWS:
        require(
            release,
            f"dispatch {workflow}",
            release_source,
            errors,
        )

        component_path = workflows / workflow
        component = component_path.read_text(encoding="utf-8")
        source = str(component_path.relative_to(root))
        require(component, 'tags:\n      - "v*"', source, errors)
        require(component, "workflow_dispatch:", source, errors)
        forbid(component, "Create and push tag", source, errors)
        forbid(component, "inputs.tag", source, errors)
        forbid(
            component,
            "github.event_name == 'workflow_dispatch'",
            source,
            errors,
        )

    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1

    print(
        "Release workflow policy verified: one tag coordinator dispatches "
        "backend, frontend and musl builds"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
