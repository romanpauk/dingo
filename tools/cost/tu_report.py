#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Report the compile cost of every translation unit in a build.

The generated test suites are template-heavy, so the useful question is not
"how long does the build take" but "which translation units cost the most".
This reads CMake's compile_commands.json, recompiles the selected translation
units, and prints the per-TU time and object size. The deterministic values
(TU count and object sizes) are the ones worth gating on; wall time is a trend.

Example:

  python tools/cost/tu_report.py \
      --compile-commands build/compile_commands.json \
      --filter generated/matrix \
      --out build/cost/tus.json
"""

from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any


def load_commands(path: Path) -> list[dict[str, str]]:
    return json.loads(path.read_text(encoding="utf-8"))


def select(
    entries: list[dict[str, str]], filters: list[str]
) -> list[dict[str, str]]:
    if not filters:
        return entries
    return [
        entry
        for entry in entries
        if any(token in entry["file"] for token in filters)
    ]


def rewrite_output(command: str, output: Path) -> list[str]:
    """Return the command's argument list with its object output redirected."""
    arguments = command.split()
    try:
        index = arguments.index("-o")
    except ValueError:
        arguments.extend(["-o", str(output)])
        return arguments
    arguments[index + 1] = str(output)
    return arguments


def object_size(path: Path) -> int:
    result = subprocess.run(
        ["size", str(path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    fields = lines[-1].split()
    return int(fields[0]) + int(fields[1]) + int(fields[2])


def measure(
    entry: dict[str, str], repetitions: int, directory: Path
) -> dict[str, Any]:
    source = Path(entry["file"])
    samples = []
    size = 0
    with tempfile.TemporaryDirectory(prefix="dingo-tu-") as temporary:
        output = Path(temporary) / "unit.o"
        for _ in range(repetitions):
            command = rewrite_output(entry["command"], output)
            start = time.perf_counter()
            subprocess.run(
                command, cwd=entry.get("directory", directory), check=True
            )
            samples.append(time.perf_counter() - start)
        size = object_size(output)
    return {
        "source": str(source),
        "seconds": samples,
        "seconds_min": min(samples),
        "object_size": size,
    }


def summarize(rows: list[dict[str, Any]]) -> dict[str, Any]:
    seconds = [row["seconds_min"] for row in rows]
    return {
        "translation_units": len(rows),
        "total_seconds": sum(seconds),
        "median_seconds": statistics.median(seconds) if seconds else 0.0,
        "max_seconds": max(seconds) if seconds else 0.0,
        "total_object_size": sum(row["object_size"] for row in rows),
    }


def report(rows: list[dict[str, Any]], limit: int) -> None:
    for row in sorted(rows, key=lambda item: -item["seconds_min"])[:limit]:
        print(
            f"{row['seconds_min']:7.2f}s {row['object_size']:8d}B "
            f"{row['source']}"
        )
    summary = summarize(rows)
    print(
        f"total {summary['total_seconds']:.1f}s over "
        f"{summary['translation_units']} translation units "
        f"(median {summary['median_seconds']:.2f}s, "
        f"max {summary['max_seconds']:.2f}s, "
        f"{summary['total_object_size']} object bytes)"
    )


def run(arguments: argparse.Namespace) -> int:
    compile_commands = arguments.compile_commands.resolve()
    entries = select(load_commands(compile_commands), arguments.filter)
    if not entries:
        raise SystemExit(f"no translation units matched {arguments.filter}")
    rows = [
        measure(entry, arguments.repetitions, compile_commands.parent)
        for entry in entries
    ]
    report(rows, arguments.limit)
    data = {"summary": summarize(rows), "translation_units": rows}
    if arguments.out is not None:
        output = arguments.out.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    return 0


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--compile-commands",
        type=Path,
        default=Path("build/compile_commands.json"),
        help="CMake compile_commands.json to read",
    )
    parser.add_argument(
        "--filter",
        action="append",
        default=[],
        help="only measure sources whose path contains this text",
    )
    parser.add_argument(
        "--repetitions", type=int, default=1, help="compile each unit N times"
    )
    parser.add_argument(
        "--limit", type=int, default=20, help="rows to print from the top"
    )
    parser.add_argument("--out", type=Path, help="write the full report as JSON")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
