#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Record and compare code-size and compile-cost probes.

The comparison uses deterministic values for the hard gate: the text, data,
and bss sizes of a compiled probe and, when Clang time-trace output is
available, the number of instantiation events. Wall time is recorded for
trends but is not a failure condition.

Example:

  python tools/cost/measure.py record \
      --include include --source test/lit/codegen-static-probes.cpp \
      --out build/cost/current.json --time-trace

  python tools/cost/measure.py compare build/cost/master.json \
      build/cost/current.json
"""

from __future__ import annotations

import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any


def run(command: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )


def object_sizes(object_path: Path) -> dict[str, int]:
    result = run(["size", str(object_path)])
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    if not lines:
        raise ValueError(f"size produced no output for {object_path}")
    fields = lines[-1].split()
    if len(fields) < 4:
        raise ValueError(f"unexpected size output for {object_path}: {lines[-1]}")
    return {
        "text": int(fields[0]),
        "data": int(fields[1]),
        "bss": int(fields[2]),
        "object": object_path.stat().st_size,
    }


def trace_summary(trace_path: Path) -> dict[str, Any]:
    if not trace_path.exists():
        return {"trace_events": 0, "dingo_trace_events": 0}
    data = json.loads(trace_path.read_text(encoding="utf-8"))
    events = data.get("traceEvents", [])
    dingo_events = [
        event
        for event in events
        if "dingo::" in str(event.get("name", ""))
        or "dingo::" in str(event.get("args", {}))
    ]
    return {
        "trace_events": len(events),
        "dingo_trace_events": len(dingo_events),
    }


def compile_once(
    cxx: str,
    source: Path,
    include: Path,
    standard: str,
    optimization: str,
    object_path: Path,
    trace_path: Path | None,
) -> float:
    command = [
        cxx,
        f"-std={standard}",
        optimization,
        f"-I{include}",
        "-c",
        str(source),
        "-o",
        str(object_path),
    ]
    if trace_path is not None:
        command.append("-ftime-trace")
    start = time.perf_counter()
    run(command, cwd=source.parent)
    elapsed = time.perf_counter() - start
    if trace_path is not None:
        # Clang writes the trace next to the object file.
        generated = object_path.with_suffix(".json")
        if generated.exists():
            generated.replace(trace_path)
    return elapsed


def preprocess_bytes(
    cxx: str, source: Path, include: Path, standard: str, optimization: str
) -> int:
    result = run(
        [
            cxx,
            f"-std={standard}",
            optimization,
            f"-I{include}",
            "-E",
            str(source),
        ],
        cwd=source.parent,
    )
    return len(result.stdout.encode("utf-8"))


def record(arguments: argparse.Namespace) -> int:
    source = arguments.source.resolve()
    include = arguments.include.resolve()
    output = arguments.out.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dingo-cost-") as directory:
        directory_path = Path(directory)
        object_path = directory_path / "probe.o"
        trace_path = directory_path / "probe.json" if arguments.time_trace else None
        samples = []
        for iteration in range(arguments.repetitions):
            iteration_object = directory_path / f"probe-{iteration}.o"
            iteration_trace = (
                directory_path / f"probe-{iteration}.json"
                if arguments.time_trace
                else None
            )
            samples.append(
                compile_once(
                    arguments.cxx,
                    source,
                    include,
                    arguments.standard,
                    arguments.optimization,
                    iteration_object,
                    iteration_trace,
                )
            )
            if iteration == arguments.repetitions - 1:
                object_path = iteration_object
                trace_path = iteration_trace
        record_data = {
            "source": str(source),
            "include": str(include),
            "cxx": arguments.cxx,
            "standard": arguments.standard,
            "optimization": arguments.optimization,
            "sizes": object_sizes(object_path),
            "compile_seconds": samples,
            "compile_seconds_median": statistics.median(samples),
            "compile_seconds_min": min(samples),
            "trace": trace_summary(trace_path) if trace_path else {},
            "headers": {
                "preprocessed_bytes": preprocess_bytes(
                    arguments.cxx,
                    source,
                    include,
                    arguments.standard,
                    arguments.optimization,
                )
            },
        }
    output.write_text(json.dumps(record_data, indent=2, sort_keys=True) + "\n")
    print(json.dumps(record_data, indent=2, sort_keys=True))
    return 0


def compare_metric(
    name: str,
    baseline: int,
    current: int,
    tolerance: float,
    problems: list[str],
) -> None:
    allowed = int(baseline * (1.0 + tolerance))
    if current > allowed:
        problems.append(f"{name}: {current} > {baseline} (+{tolerance:.1%})")


def compare(arguments: argparse.Namespace) -> int:
    baseline = json.loads(arguments.baseline.read_text(encoding="utf-8"))
    current = json.loads(arguments.current.read_text(encoding="utf-8"))
    problems: list[str] = []
    for metric in ("text", "data", "bss"):
        compare_metric(
            metric,
            int(baseline["sizes"][metric]),
            int(current["sizes"][metric]),
            arguments.size_tolerance,
            problems,
        )
    baseline_trace = baseline.get("trace", {})
    current_trace = current.get("trace", {})
    if baseline_trace and current_trace:
        compare_metric(
            "dingo_trace_events",
            int(baseline_trace.get("dingo_trace_events", 0)),
            int(current_trace.get("dingo_trace_events", 0)),
            arguments.instantiation_tolerance,
            problems,
        )
    baseline_headers = baseline.get("headers", {})
    current_headers = current.get("headers", {})
    if baseline_headers and current_headers:
        compare_metric(
            "preprocessed_bytes",
            int(baseline_headers.get("preprocessed_bytes", 0)),
            int(current_headers.get("preprocessed_bytes", 0)),
            arguments.header_tolerance,
            problems,
        )
    print(
        "compile seconds median: "
        f"{baseline.get('compile_seconds_median', 0):.3f} -> "
        f"{current.get('compile_seconds_median', 0):.3f}"
    )
    for problem in problems:
        print(f"FAIL: {problem}", file=sys.stderr)
    return 1 if problems else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(required=True)

    record_parser = subparsers.add_parser("record")
    record_parser.add_argument("--include", type=Path, required=True)
    record_parser.add_argument("--source", type=Path, required=True)
    record_parser.add_argument("--out", type=Path, required=True)
    record_parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    record_parser.add_argument("--standard", default="c++17")
    record_parser.add_argument("--optimization", default="-O2")
    record_parser.add_argument("--repetitions", type=int, default=3)
    record_parser.add_argument("--time-trace", action="store_true")
    record_parser.set_defaults(handler=record)

    compare_parser = subparsers.add_parser("compare")
    compare_parser.add_argument("baseline", type=Path)
    compare_parser.add_argument("current", type=Path)
    compare_parser.add_argument("--size-tolerance", type=float, default=0.0)
    compare_parser.add_argument("--instantiation-tolerance", type=float, default=0.0)
    compare_parser.add_argument("--header-tolerance", type=float, default=0.0)
    compare_parser.set_defaults(handler=compare)

    arguments = parser.parse_args()
    return arguments.handler(arguments)


if __name__ == "__main__":
    raise SystemExit(main())
