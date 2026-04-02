#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys

SEMVER_PATTERN = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")
RELEASE_LINE_PATTERN = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)$")
RELEASE_BRANCH_PATTERN = re.compile(r"^releases/((0|[1-9]\d*)\.(0|[1-9]\d*))$")
PROJECT_PATTERN = re.compile(r"^project\(dingo(?:\s+VERSION\s+[^\s\)]+)?\)$")


def fail(message: str) -> None:
    raise ValueError(message)


def parse_version(version: str) -> tuple[int, int, int]:
    match = SEMVER_PATTERN.fullmatch(version)
    if not match:
        fail(
            f"invalid version '{version}': expected semantic version in x.y.z form"
        )
    return tuple(int(part) for part in match.groups())


def derive_release_line(version: str) -> str:
    major, minor, _patch = parse_version(version)
    return f"{major}.{minor}"


def normalize_release_line(version: str, release_line: str | None) -> str:
    derived = derive_release_line(version)
    if release_line is None or release_line == "":
        return derived
    if not RELEASE_LINE_PATTERN.fullmatch(release_line):
        fail(
            f"invalid release line '{release_line}': expected x.y form"
        )
    if release_line != derived:
        fail(
            f"release line '{release_line}' does not match version '{version}'"
        )
    return release_line


def git_ref_exists(ref: str) -> bool:
    return (
        subprocess.run(
            ["git", "show-ref", "--verify", "--quiet", ref],
            check=False,
        ).returncode
        == 0
    )


def classify_ref_state(expected_commit: str, actual_commit: str | None) -> str:
    if actual_commit is None:
        return "missing"
    if actual_commit == expected_commit:
        return "matches"
    return "conflict"


def build_release_plan(
    version: str,
    source_ref: str,
    release_line: str | None,
    *,
    release_branch_exists: bool,
    tag_exists: bool,
) -> dict[str, str]:
    major, minor, patch = parse_version(version)
    normalized_line = normalize_release_line(version, release_line)
    release_branch = f"releases/{normalized_line}"
    tag = f"v{version}"

    if source_ref == "master":
        if patch != 0:
            fail(
                "releases from master must create a new release line and use x.y.0"
            )
        source_kind = "master"
    else:
        branch_match = RELEASE_BRANCH_PATTERN.fullmatch(source_ref)
        if not branch_match:
            fail(
                "source_ref must be 'master' or a maintenance branch in "
                "'releases/x.y' form"
            )
        source_kind = "release_branch"
        if branch_match.group(1) != normalized_line:
            fail(
                f"source_ref '{source_ref}' does not match release line "
                f"'{normalized_line}'"
            )

    return {
        "version": version,
        "release_line": normalized_line,
        "release_branch": release_branch,
        "source_ref": source_ref,
        "source_kind": source_kind,
        "tag": tag,
        "major": str(major),
        "minor": str(minor),
        "patch": str(patch),
        "release_branch_exists": "true" if release_branch_exists else "false",
        "tag_exists": "true" if tag_exists else "false",
    }


def set_cmake_version(path: pathlib.Path, version: str) -> bool:
    parse_version(version)
    lines = path.read_text().splitlines()
    for index, line in enumerate(lines):
        if PROJECT_PATTERN.fullmatch(line):
            replacement = f"project(dingo VERSION {version})"
            if line == replacement:
                return False

            lines[index] = replacement
            path.write_text("\n".join(lines) + "\n")
            return True

    fail(f"could not find a replaceable project(dingo ...) line in {path}")


def command_plan(args: argparse.Namespace) -> int:
    release_branch = f"releases/{normalize_release_line(args.version, args.release_line)}"
    plan = build_release_plan(
        args.version,
        args.source_ref,
        args.release_line,
        release_branch_exists=git_ref_exists(f"refs/remotes/origin/{release_branch}")
        or git_ref_exists(f"refs/heads/{release_branch}"),
        tag_exists=git_ref_exists(f"refs/tags/v{args.version}"),
    )
    for key, value in plan.items():
        print(f"{key}={value}")
    return 0


def command_set_version(args: argparse.Namespace) -> int:
    changed = set_cmake_version(pathlib.Path(args.file), args.version)
    print(f"changed={'true' if changed else 'false'}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Release workflow helpers")
    subparsers = parser.add_subparsers(dest="command", required=True)

    plan_parser = subparsers.add_parser(
        "plan", help="validate release inputs and emit workflow metadata"
    )
    plan_parser.add_argument("--version", required=True)
    plan_parser.add_argument("--source-ref", required=True)
    plan_parser.add_argument("--release-line")
    plan_parser.set_defaults(func=command_plan)

    set_version_parser = subparsers.add_parser(
        "set-version", help="update the root CMake project version"
    )
    set_version_parser.add_argument("--file", required=True)
    set_version_parser.add_argument("--version", required=True)
    set_version_parser.set_defaults(func=command_set_version)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        return args.func(args)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
