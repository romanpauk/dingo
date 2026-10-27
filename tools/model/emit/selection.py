#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate the exhaustive selection-policy probes.

The two-source selection rule is small enough to write by hand, which is
exactly why a missed cell is easy to introduce. The model enumerates every
policy, primary status, and secondary status, evaluates a reference rule, and
emits one assertion per cell.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template


STATUSES = ("found", "not_found", "ambiguous")
POLICIES = ("prefer_primary", "ambiguous_on_conflict")
RESULTS = ("primary", "secondary", "missing", "ambiguous")


@dataclass(frozen=True)
class Row:
    policy: str
    primary: str
    secondary: str
    result: str
    status: str


def resolve(primary: str, secondary: str, policy: str) -> str:
    if policy == "prefer_primary":
        if primary == "ambiguous":
            return "ambiguous"
        if primary == "found":
            return "primary"
        if secondary == "ambiguous":
            return "ambiguous"
        if secondary == "found":
            return "secondary"
        return "missing"

    if (
        primary == "ambiguous"
        or secondary == "ambiguous"
        or (primary == "found" and secondary == "found")
    ):
        return "ambiguous"
    if primary == "found":
        return "primary"
    if secondary == "found":
        return "secondary"
    return "missing"


def result_status(result: str) -> str:
    if result in ("primary", "secondary"):
        return "found"
    if result == "ambiguous":
        return "ambiguous"
    return "not_found"


def build_rows() -> list[Row]:
    rows = []
    for policy in POLICIES:
        for primary in STATUSES:
            for secondary in STATUSES:
                result = resolve(primary, secondary, policy)
                rows.append(
                    Row(
                        policy=policy,
                        primary=primary,
                        secondary=secondary,
                        result=result,
                        status=result_status(result),
                    )
                )
    return rows


def validate() -> None:
    rows = build_rows()
    if len(rows) != len(POLICIES) * len(STATUSES) * len(STATUSES):
        raise ValueError("selection product is incomplete")
    keys = {(row.policy, row.primary, row.secondary) for row in rows}
    if len(keys) != len(rows):
        raise ValueError("selection product contains duplicate cells")
    for row in rows:
        if row.result not in RESULTS:
            raise ValueError(f"unknown result {row.result!r}")
        if row.status not in STATUSES:
            raise ValueError(f"unknown status {row.status!r}")


def _enum_names(text: str, name: str) -> set[str]:
    match = re.search(
        rf"enum\s+class\s+{name}\s*\{{(.*?)\}};",
        re.sub(r"//[^\n]*", "", text),
        re.DOTALL,
    )
    if match is None:
        raise ValueError(f"enum {name} not found")
    return {item.strip() for item in match.group(1).split(",") if item.strip()}


def validate_catalog(include_directory: Path) -> None:
    validate()
    policy_header = (
        include_directory / "dingo" / "binding" / "binding_resolution_policy.h"
    )
    selection_header = (
        include_directory / "dingo" / "binding" / "binding_selection.h"
    )
    policy_text = policy_header.read_text(encoding="utf-8")
    selection_text = selection_header.read_text(encoding="utf-8")
    problems = []
    if _enum_names(policy_text, "binding_resolution_policy") != set(POLICIES):
        problems.append("binding_resolution_policy")
    if _enum_names(policy_text, "binding_result") != set(RESULTS):
        problems.append("binding_result")
    if _enum_names(selection_text, "binding_status") != set(STATUSES):
        problems.append("binding_status")
    if problems:
        raise ValueError(
            "selection catalog is out of date: " + ", ".join(problems)
        )


def render(output: Path) -> str:
    validate()
    return render_template("selection_rules.cpp.j2", rows=build_rows(), results=RESULTS)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    emit(arguments.out, render(arguments.out), check=arguments.check)


if __name__ == "__main__":
    main()
