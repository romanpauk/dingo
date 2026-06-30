#!/usr/bin/env python3

import os
import platform
import re
import sys
from pathlib import Path


PROBE_LIMITS = {
    "probe_static_resolution_consumer_read": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_static_resolution_mixed_container_consumer_read": {
        "default": 0x50,
        "gcc": 0x60,
        "gcc_arm64": 0x60,
    },
    "probe_runtime_resolution_consumer_read": {
        "default": None,
    },
    "probe_static_resolution_shared_config": {
        "default": 0x90,
        "clang_arm64": 0xd0,
    },
    "probe_static_resolution_mixed_container_shared_config": {
        "default": 0x120,
        "clang_arm64": 0x140,
    },
    "probe_runtime_resolution_shared_config": {
        "default": None,
    },
    "probe_static_resolution_shared_value_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_static_resolution_mixed_container_shared_value_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_runtime_resolution_shared_value_config": {
        "default": None,
    },
    "probe_static_resolution_shared_reference_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_static_resolution_mixed_container_shared_reference_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc13": 0xa0,
        "gcc_arm64": 0x60,
    },
    "probe_runtime_resolution_shared_reference_config": {
        "default": None,
    },
    "probe_static_resolution_optional_config": {
        "default": 0x60,
    },
    "probe_static_resolution_mixed_container_optional_config": {
        "default": 0x10,
    },
    "probe_runtime_resolution_optional_config": {
        "default": None,
    },
    "probe_static_resolution_unique_value_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_runtime_resolution_unique_value_config": {
        "default": None,
    },
    "probe_static_resolution_unique_rvalue_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_static_resolution_mixed_container_unique_value_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_static_resolution_mixed_container_unique_rvalue_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc13": 0x90,
        "gcc_arm64": 0x60,
    },
    "probe_runtime_resolution_unique_rvalue_config": {
        "default": None,
    },
    "probe_static_resolution_unique_wrapper_config": {
        "default": 0x10,
        "gcc": 0x50,
        "gcc_arm64": 0x60,
    },
    "probe_static_resolution_mixed_container_unique_wrapper_config": {
        "default": 0x10,
    },
    "probe_runtime_resolution_unique_wrapper_config": {
        "default": None,
    },
    "probe_static_resolution_interface_handle": {
        "default": 0x180,
        "clang": 0x1a0,
        "clang_arm64": 0x1d0,
    },
    "probe_static_resolution_mixed_container_interface_handle": {
        "default": 0x200,
        "clang": 0x260,
        "gcc_arm64": 0x360,
    },
    "probe_runtime_resolution_interface_handle": {
        "default": None,
    },
    "probe_static_resolution_collection_sum": {
        "default": 0x500,
    },
    "probe_static_resolution_mixed_container_collection_sum": {
        "default": 0x800,
    },
    "probe_runtime_resolution_collection_sum": {
        "default": None,
    },
    "probe_runtime_resolution_external_value_storage": {
        "default": 0x300,
        "clang": 0x420,
        "clang_arm64": 0x350,
        "gcc": 0x310,
        "gcc13": 0x540,
        "gcc14": 0x590,
        "gcc15": 0x590,
        "gcc_arm64": 0x590,
    },
    "probe_runtime_resolution_mixed_container_external_value_storage": {
        "default": 0x520,
        "gcc14": 0x5d0,
        "gcc15": 0x580,
        "gcc_arm64": 0x560,
    },
    "probe_runtime_resolution_external_reference_storage": {
        "default": 0x300,
        "clang": 0x440,
        "clang_arm64": 0x370,
        "gcc": 0x330,
        "gcc13": 0x550,
        "gcc14": 0x5a0,
        "gcc15": 0x5a0,
        "gcc_arm64": 0x5a0,
    },
    "probe_runtime_resolution_mixed_container_external_reference_storage": {
        "default": 0x520,
        "gcc14": 0x5c0,
        "gcc15": 0x560,
    },
    "probe_runtime_resolution_external_wrapper_storage": {
        "default": 0x420,
        "clang": 0x940,
        "clang_arm64": 0x740,
        "gcc": 0x750,
        "gcc13": 0x750,
        "gcc14": 0x750,
        "gcc15": 0x750,
        "gcc_arm64": 0x750,
    },
    "probe_runtime_resolution_mixed_container_external_wrapper_storage": {
        "default": 0x580,
        "clang": 0x930,
        "gcc13": 0x640,
        "gcc14": 0x640,
        "gcc15": 0x640,
        "gcc_arm64": 0x6a0,
    },
}

EXPECTED_FASTER_THAN_RUNTIME = (
    ("probe_static_resolution_consumer_read", "probe_runtime_resolution_consumer_read"),
    ("probe_static_resolution_shared_value_config", "probe_runtime_resolution_shared_value_config"),
    (
        "probe_static_resolution_shared_reference_config",
        "probe_runtime_resolution_shared_reference_config",
    ),
    ("probe_static_resolution_unique_value_config", "probe_runtime_resolution_unique_value_config"),
    ("probe_static_resolution_unique_rvalue_config", "probe_runtime_resolution_unique_rvalue_config"),
    (
        "probe_static_resolution_unique_wrapper_config",
        "probe_runtime_resolution_unique_wrapper_config",
    ),
    ("probe_static_resolution_interface_handle", "probe_runtime_resolution_interface_handle"),
    ("probe_static_resolution_collection_sum", "probe_runtime_resolution_collection_sum"),
)

SHAPES = {
    "consumer_read": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "shared_config": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "shared_value_config": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "shared_reference_config": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "optional_config": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "unique_value_config": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "unique_rvalue_config": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "unique_wrapper_config": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "interface_handle": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "collection_sum": (
        "static_resolution",
        "static_resolution_mixed_container",
        "runtime_resolution",
    ),
    "external_value_storage": (
        "runtime_resolution_mixed_container",
        "runtime_resolution",
    ),
    "external_reference_storage": (
        "runtime_resolution_mixed_container",
        "runtime_resolution",
    ),
    "external_wrapper_storage": (
        "runtime_resolution_mixed_container",
        "runtime_resolution",
    ),
}

EXPECTED_SYMBOLS = {
    f"probe_{prefix}_{shape}"
    for shape, prefixes in SHAPES.items()
    for prefix in prefixes
}

SYMBOL_RE = re.compile(
    r"^[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\S\s+(probe_[a-z0-9_]+)$"
)


def expected_max_for_environment() -> dict[str, int]:
    compiler_id = os.environ.get("DINGO_CXX_ID", "")
    compiler_family = os.environ.get("DINGO_CXX_FAMILY", "")
    compiler_major = os.environ.get("DINGO_CXX_VERSION_MAJOR", "")
    machine = platform.machine().lower()
    is_clang = compiler_family == "clang" or "clang" in compiler_id
    is_gcc = compiler_family == "gcc" or ("g++" in compiler_id and not is_clang)
    is_arm64 = machine in ("aarch64", "arm64")

    columns = ["default"]
    if is_clang:
        columns.append("clang")
        if is_arm64:
            columns.append("clang_arm64")
    if is_gcc:
        columns.append("gcc")
    if compiler_major == "13" or "g++-13" in compiler_id:
        columns.append("gcc13")
    if compiler_major == "14" or "g++-14" in compiler_id:
        columns.append("gcc14")
    if compiler_major == "15" or "g++-15" in compiler_id:
        columns.append("gcc15")
    if is_gcc and is_arm64:
        columns.append("gcc_arm64")

    expected_max: dict[str, int] = {}
    for symbol, limits in PROBE_LIMITS.items():
        limit = limits["default"]
        for column in columns[1:]:
            limit = limits.get(column, limit)
        if limit is not None:
            expected_max[symbol] = limit
    return expected_max


def load_sizes(path: Path) -> dict[str, int]:
    sizes: dict[str, int] = {}
    for line in path.read_text().splitlines():
        match = SYMBOL_RE.match(line.strip())
        if match is None or match.group(2) not in EXPECTED_SYMBOLS:
            continue
        sizes[match.group(2)] = int(match.group(1), 16)
    return sizes


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_codegen_probe_sizes.py <nm-output>", file=sys.stderr)
        return 2

    sizes = load_sizes(Path(sys.argv[1]))
    unexpected_limits = sorted(PROBE_LIMITS.keys() - EXPECTED_SYMBOLS)
    missing_limits = sorted(EXPECTED_SYMBOLS - PROBE_LIMITS.keys())
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

    missing = sorted(EXPECTED_SYMBOLS - sizes.keys())
    if missing:
        print(f"missing probe symbols: {', '.join(missing)}", file=sys.stderr)
        return 1

    expected_max = expected_max_for_environment()

    failed = False
    for symbol, max_size in expected_max.items():
        size = sizes[symbol]
        if size > max_size:
            print(
                f"{symbol} regressed: size 0x{size:x} exceeds 0x{max_size:x}",
                file=sys.stderr,
            )
            failed = True

    for faster_symbol, slower_symbol in EXPECTED_FASTER_THAN_RUNTIME:
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
