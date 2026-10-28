#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Pin and compare model outcomes.

The matrix probe writes one JSON line per scope/form/shape row. The behavior
catalog contributes the expected request and construction/copy/move counts for
every generated behavior case. Together they are a differential baseline: a
change is allowed only when the baseline is updated with a declared reason.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


def read_matrix(path: Path) -> dict[str, dict[str, Any]]:
    rows = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValueError(f"{path}:{number}: invalid JSON: {error}") from error
        key = f"{row['scope']}/{row['form']}/{row['shape']}"
        if key in rows:
            raise ValueError(f"{path}:{number}: duplicate row {key}")
        rows[key] = {
            "direct": bool(row["direct"]),
            "lifted": bool(row["lifted"]),
            "top_level": bool(row["top_level"]),
            "reason": str(row["reason"]),
        }
    if not rows:
        raise ValueError(f"{path}: no matrix rows")
    return rows


def read_behavior(path: Path) -> dict[str, dict[str, Any]]:
    rows = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as error:
            raise ValueError(f"{path}:{number}: invalid JSON: {error}") from error
        key = str(row["name"])
        if key in rows:
            raise ValueError(f"{path}:{number}: duplicate row {key}")
        rows[key] = {
            "scope": str(row["scope"]),
            "form": str(row["form"]),
            "container": str(row["container"]),
            "request": str(row["request"]),
            "result": str(row["result"]),
            "constructions": int(row["constructions"]),
            "copies": int(row["copies"]),
            "moves": int(row["moves"]),
            "destructions": int(row["destructions"]),
        }
    if not rows:
        raise ValueError(f"{path}: no behavior rows")
    return rows


def build_outcomes(
    matrix: Path, behavior: Path | list[Path]
) -> dict[str, dict[str, dict[str, Any]]]:
    paths = behavior if isinstance(behavior, list) else [behavior]
    rows: dict[str, dict[str, Any]] = {}
    for path in paths:
        for key, row in read_behavior(path).items():
            if key in rows:
                raise ValueError(f"duplicate behavior row {key} in {path}")
            rows[key] = row
    return {"matrix": read_matrix(matrix), "behavior": rows}


def diff_outcomes(
    baseline: dict[str, dict[str, dict[str, Any]]],
    current: dict[str, dict[str, dict[str, Any]]],
) -> list[str]:
    differences = []
    for section in ("matrix", "behavior"):
        baseline_rows = baseline.get(section, {})
        current_rows = current.get(section, {})
        for key in sorted(set(baseline_rows) | set(current_rows)):
            if key not in baseline_rows:
                differences.append(f"+ {section} {key}")
            elif key not in current_rows:
                differences.append(f"- {section} {key}")
            elif baseline_rows[key] != current_rows[key]:
                differences.append(
                    f"~ {section} {key}: "
                    f"{baseline_rows[key]} -> {current_rows[key]}"
                )
    return differences


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def record(arguments: argparse.Namespace) -> int:
    outcomes = build_outcomes(arguments.matrix, arguments.behavior)
    write_json(arguments.out, outcomes)
    print(
        f"recorded {len(outcomes['matrix'])} matrix rows and "
        f"{len(outcomes['behavior'])} behavior rows"
    )
    return 0


def compare(
    baseline: dict[str, dict[str, dict[str, Any]]],
    current: dict[str, dict[str, dict[str, Any]]],
) -> list[str]:
    return diff_outcomes(baseline, current)


def check(arguments: argparse.Namespace) -> int:
    baseline = json.loads(arguments.baseline.read_text(encoding="utf-8"))
    current = build_outcomes(arguments.matrix, arguments.behavior)
    differences = compare(baseline, current)
    if not differences:
        print("model outcomes match the baseline")
        return 0
    print(f"{len(differences)} model outcome difference(s):", file=sys.stderr)
    for difference in differences[:40]:
        print(f"  {difference}", file=sys.stderr)
    if len(differences) > 40:
        print(f"  ... {len(differences) - 40} more", file=sys.stderr)
    print(
        "If this is intended, run:\n"
        "  python -m spec.report.baseline update "
        f"--matrix {arguments.matrix} "
        "--behavior <path> [--behavior <path> ...] "
        f"--baseline {arguments.baseline} "
        "--changes test/baselines/model-changes.json --reason \"...\"",
        file=sys.stderr,
    )
    return 1


def update(arguments: argparse.Namespace) -> int:
    if not arguments.reason.strip():
        raise SystemExit("an update requires a non-empty --reason")
    current = build_outcomes(arguments.matrix, arguments.behavior)
    if arguments.baseline.exists():
        baseline = json.loads(arguments.baseline.read_text(encoding="utf-8"))
        differences = compare(baseline, current)
    else:
        differences = [f"+ {section}" for section in ("matrix", "behavior")]
    write_json(arguments.baseline, current)
    changes = []
    if arguments.changes.exists():
        changes = json.loads(arguments.changes.read_text(encoding="utf-8"))
    changes.append(
        {
            "reason": arguments.reason,
            "differences": differences,
        }
    )
    write_json(arguments.changes, changes)
    print(f"updated {arguments.baseline} ({len(differences)} differences)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(required=True)

    record_parser = subparsers.add_parser("record")
    record_parser.add_argument("--matrix", type=Path, required=True)
    record_parser.add_argument(
        "--behavior", type=Path, action="append", required=True
    )
    record_parser.add_argument("--out", type=Path, required=True)
    record_parser.set_defaults(handler=record)

    check_parser = subparsers.add_parser("check")
    check_parser.add_argument("--matrix", type=Path, required=True)
    check_parser.add_argument(
        "--behavior", type=Path, action="append", required=True
    )
    check_parser.add_argument("--baseline", type=Path, required=True)
    check_parser.set_defaults(handler=check)

    update_parser = subparsers.add_parser("update")
    update_parser.add_argument("--matrix", type=Path, required=True)
    update_parser.add_argument(
        "--behavior", type=Path, action="append", required=True
    )
    update_parser.add_argument("--baseline", type=Path, required=True)
    update_parser.add_argument("--changes", type=Path, required=True)
    update_parser.add_argument("--reason", required=True)
    update_parser.set_defaults(handler=update)

    arguments = parser.parse_args()
    return arguments.handler(arguments)


if __name__ == "__main__":
    raise SystemExit(main())
