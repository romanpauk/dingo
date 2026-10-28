#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Collect reference edges with the astrefs tool and report unused declarations.

The tool parses one translation unit at a time and writes one JSON object per
declaration and per edge. This driver picks the translation units that use the
library — the hand-written tests and the examples — and summarises the result as
"no declaration of this name has an incoming edge".

Parsing every generated test would take far longer and add no declaration that
the library headers do not already contain.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


def translation_units(repository: Path) -> list[Path]:
    units: list[Path] = []
    for directory in ("test", "examples"):
        base = repository / directory
        if not base.is_dir():
            continue
        for source in sorted(base.rglob("*.cpp")):
            if "generated" in source.parts or "build" in source.parts:
                continue
            units.append(source.relative_to(repository))
    return units


def run(
    astrefs: Path, repository: Path, compile_commands: Path, units: list[Path],
    root: str,
) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for index, unit in enumerate(units, start=1):
        result = subprocess.run(
            [
                str(astrefs),
                f"--root={root}",
                f"-p={compile_commands}",
                f"--source={unit}",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            print(f"# skipped {unit}", file=sys.stderr)
            continue
        for line in result.stdout.splitlines():
            if line.startswith("{"):
                rows.append(json.loads(line))
        if index % 10 == 0:
            print(f"# {index}/{len(units)}", file=sys.stderr)
    return rows


def classify(rows: list[dict[str, object]]) -> tuple[int, list[str]]:
    declarations: dict[int, dict[str, object]] = {}
    incoming: dict[int, int] = defaultdict(int)
    for row in rows:
        if row["kind"] == "decl":
            declarations[int(row["id"])] = row
        elif row["kind"] == "edge":
            incoming[int(row["to"])] += 1

    groups: dict[str, list[dict[str, object]]] = defaultdict(list)
    for identifier, declaration in declarations.items():
        name = str(declaration["name"])
        if not name or name[0].isdigit():
            continue
        declaration["references"] = incoming[identifier]
        groups[name.split("<", 1)[0]].append(declaration)

    unused = [
        name
        for name, members in groups.items()
        if max(int(member["references"]) for member in members) == 0
    ]
    return len(declarations), sorted(unused)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--astrefs", type=Path, required=True)
    parser.add_argument("--compile-commands", type=Path, default=Path("build"))
    parser.add_argument("--root", default="include/dingo")
    parser.add_argument("--out", type=Path)
    parser.add_argument("--limit", type=int, default=0)
    arguments = parser.parse_args()

    repository = Path(".").resolve()
    units = translation_units(repository)
    if arguments.limit:
        units = units[: arguments.limit]
    rows = run(
        arguments.astrefs.resolve(),
        repository,
        arguments.compile_commands.resolve(),
        units,
        arguments.root,
    )
    if arguments.out is not None:
        arguments.out.write_text(
            "\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8"
        )
    total, unused = classify(rows)
    print(f"## Declarations with no incoming edge ({len(unused)} of {total})\n")
    for name in unused:
        print(f"- `{name}`")


if __name__ == "__main__":
    main()
