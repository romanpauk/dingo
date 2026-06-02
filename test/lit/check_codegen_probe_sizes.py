#!/usr/bin/env python3

import os
import platform
import re
import runpy
import sys
from pathlib import Path


SYMBOL_RE = re.compile(
    r"^[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\S\s+(probe_[a-z0-9_]+)$"
)


def load_expectations(path: Path) -> tuple[dict, tuple, dict]:
    expectations = runpy.run_path(str(path))
    probe_limits = expectations["PROBE_LIMITS"]
    faster_than_runtime = expectations["EXPECTED_FASTER_THAN_RUNTIME"]
    shapes = expectations["SHAPES"]
    expected_symbols = {
        f"probe_{prefix}_{shape}"
        for shape, prefixes in shapes.items()
        for prefix in prefixes
    }
    return probe_limits, faster_than_runtime, expected_symbols


def expected_max_for_environment(probe_limits: dict[str, dict]) -> dict[str, int]:
    compiler_id = os.environ.get("DINGO_CXX_ID", "")
    machine = platform.machine().lower()
    is_clang = "clang" in compiler_id
    is_gcc = "g++" in compiler_id and not is_clang
    is_arm64 = machine in ("aarch64", "arm64")

    columns = ["default"]
    if is_clang:
        columns.append("clang")
        if is_arm64:
            columns.append("clang_arm64")
    if is_gcc:
        columns.append("gcc")
    if "g++-13" in compiler_id:
        columns.append("gcc13")
    if "g++-14" in compiler_id:
        columns.append("gcc14")
    if "g++-15" in compiler_id:
        columns.append("gcc15")
    if is_gcc and is_arm64:
        columns.append("gcc_arm64")

    expected_max: dict[str, int] = {}
    for symbol, limits in probe_limits.items():
        limit = limits["default"]
        for column in columns[1:]:
            limit = limits.get(column, limit)
        if limit is not None:
            expected_max[symbol] = limit
    return expected_max


def load_sizes(path: Path, expected_symbols: set[str]) -> dict[str, int]:
    sizes: dict[str, int] = {}
    for line in path.read_text().splitlines():
        match = SYMBOL_RE.match(line.strip())
        if match is None or match.group(2) not in expected_symbols:
            continue
        sizes[match.group(2)] = int(match.group(1), 16)
    return sizes


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "usage: check_codegen_probe_sizes.py <nm-output> <expectations.py>",
            file=sys.stderr,
        )
        return 2

    probe_limits, faster_than_runtime, expected_symbols = load_expectations(
        Path(sys.argv[2])
    )
    sizes = load_sizes(Path(sys.argv[1]), expected_symbols)
    unexpected_limits = sorted(probe_limits.keys() - expected_symbols)
    missing_limits = sorted(expected_symbols - probe_limits.keys())
    if unexpected_limits or missing_limits:
        if unexpected_limits:
            print(
                f"unexpected probe limits: {', '.join(unexpected_limits)}",
                file=sys.stderr,
            )
        if missing_limits:
            print(
                f"missing probe limits: {', '.join(missing_limits)}",
                file=sys.stderr,
            )
        return 1

    missing = sorted(expected_symbols - sizes.keys())
    if missing:
        print(f"missing probe symbols: {', '.join(missing)}", file=sys.stderr)
        return 1

    expected_max = expected_max_for_environment(probe_limits)

    failed = False
    for symbol, max_size in expected_max.items():
        size = sizes[symbol]
        if size > max_size:
            print(
                f"{symbol} regressed: size 0x{size:x} exceeds 0x{max_size:x}",
                file=sys.stderr,
            )
            failed = True

    for faster_symbol, slower_symbol in faster_than_runtime:
        if sizes[faster_symbol] > sizes[slower_symbol]:
            print(
                f"{faster_symbol} regressed past runtime path: "
                f"0x{sizes[faster_symbol]:x} > 0x{sizes[slower_symbol]:x}",
                file=sys.stderr,
            )
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
