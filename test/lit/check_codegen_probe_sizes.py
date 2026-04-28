#!/usr/bin/env python3

import os
import platform
import re
import sys
from pathlib import Path


EXPECTED_MAX = {
    "probe_static_service_read": 0x10,
    "probe_hybrid_service_read": 0x250,
    "probe_static_shared_config": 0x90,
    "probe_hybrid_shared_config": 0x220,
    "probe_static_shared_value_config": 0x10,
    "probe_hybrid_shared_value_config": 0x1e0,
    "probe_static_shared_reference_config": 0x10,
    "probe_hybrid_shared_reference_config": 0x1d0,
    "probe_static_optional_config": 0x60,
    "probe_hybrid_optional_config": 0x1e0,
    "probe_static_unique_value_config": 0x10,
    "probe_static_unique_rvalue_config": 0x10,
    "probe_hybrid_unique_value_config": 0x1c0,
    "probe_hybrid_unique_rvalue_config": 0x140,
    "probe_static_unique_wrapper_config": 0x10,
    "probe_hybrid_unique_wrapper_config": 0x1c0,
    "probe_static_interface_handle": 0x180,
    "probe_hybrid_interface_handle": 0x320,
    "probe_static_collection_sum": 0x500,
    "probe_hybrid_collection_sum": 0x800,
    "probe_runtime_external_value_storage": 0x280,
    "probe_hybrid_external_value_storage": 0x520,
    "probe_runtime_external_reference_storage": 0x280,
    "probe_hybrid_external_reference_storage": 0x520,
    "probe_runtime_external_wrapper_storage": 0x420,
    "probe_hybrid_external_wrapper_storage": 0x580,
}

CLANG_EXPECTED_MAX = {
    "probe_static_interface_handle": 0x1a0,
    "probe_runtime_external_value_storage": 0x2b0,
    "probe_runtime_external_reference_storage": 0x2c0,
    "probe_runtime_external_wrapper_storage": 0x840,
    "probe_hybrid_external_wrapper_storage": 0x840,
}

CLANG_ARM64_EXPECTED_MAX = {
    "probe_static_shared_config": 0xd0,
    "probe_static_interface_handle": 0x1d0,
    "probe_runtime_external_wrapper_storage": 0x680,
}

GCC_TINY_PROBE_EXPECTED_MAX = {
    "probe_static_service_read": 0x50,
    "probe_static_shared_value_config": 0x50,
    "probe_static_shared_reference_config": 0x50,
    "probe_static_unique_value_config": 0x50,
    "probe_static_unique_rvalue_config": 0x50,
    "probe_static_unique_wrapper_config": 0x50,
}

GCC_ARM64_TINY_PROBE_EXPECTED_MAX = {
    symbol: 0x60 for symbol in GCC_TINY_PROBE_EXPECTED_MAX
}

GCC_ARM64_EXPECTED_MAX = {
    **GCC_ARM64_TINY_PROBE_EXPECTED_MAX,
    "probe_hybrid_interface_handle": 0x360,
    "probe_hybrid_external_value_storage": 0x560,
    "probe_runtime_external_wrapper_storage": 0x480,
}

GCC_HYBRID_PROBE_EXPECTED_MAX = {
    # GCC's symbol sizes include version-specific layout and padding noise.
    # The static tiny-probe caps and static-vs-runtime checks still guard the
    # optimized path; these limits only keep hybrid probes portable across GCC.
    "probe_hybrid_shared_reference_config": 0x1e0,
    "probe_hybrid_unique_rvalue_config": 0x160,
}

GCC13_EXPECTED_MAX = {
    **GCC_TINY_PROBE_EXPECTED_MAX,
    **GCC_HYBRID_PROBE_EXPECTED_MAX,
}

GCC14_EXPECTED_MAX = {
    **GCC_TINY_PROBE_EXPECTED_MAX,
    **GCC_HYBRID_PROBE_EXPECTED_MAX,
    "probe_hybrid_service_read": 0x280,
    "probe_hybrid_interface_handle": 0x360,
    "probe_hybrid_external_value_storage": 0x5d0,
    "probe_hybrid_external_reference_storage": 0x5c0,
    "probe_hybrid_external_wrapper_storage": 0x620,
}

GCC15_EXPECTED_MAX = {
    **GCC_TINY_PROBE_EXPECTED_MAX,
    **GCC_HYBRID_PROBE_EXPECTED_MAX,
    "probe_hybrid_service_read": 0x280,
    "probe_hybrid_external_value_storage": 0x580,
    "probe_hybrid_external_reference_storage": 0x560,
    "probe_hybrid_external_wrapper_storage": 0x5c0,
}

EXPECTED_FASTER_THAN_RUNTIME = (
    ("probe_static_service_read", "probe_runtime_service_read"),
    ("probe_static_shared_value_config", "probe_runtime_shared_value_config"),
    (
        "probe_static_shared_reference_config",
        "probe_runtime_shared_reference_config",
    ),
    ("probe_static_unique_value_config", "probe_runtime_unique_value_config"),
    ("probe_static_unique_rvalue_config", "probe_runtime_unique_rvalue_config"),
    (
        "probe_static_unique_wrapper_config",
        "probe_runtime_unique_wrapper_config",
    ),
    ("probe_static_interface_handle", "probe_runtime_interface_handle"),
    ("probe_static_collection_sum", "probe_runtime_collection_sum"),
)

SHAPES = {
    "service_read": ("static", "hybrid", "runtime"),
    "shared_config": ("static", "hybrid", "runtime"),
    "shared_value_config": ("static", "hybrid", "runtime"),
    "shared_reference_config": ("static", "hybrid", "runtime"),
    "optional_config": ("static", "hybrid", "runtime"),
    "unique_value_config": ("static", "hybrid", "runtime"),
    "unique_rvalue_config": ("static", "hybrid", "runtime"),
    "unique_wrapper_config": ("static", "hybrid", "runtime"),
    "interface_handle": ("static", "hybrid", "runtime"),
    "collection_sum": ("static", "hybrid", "runtime"),
    "external_value_storage": ("hybrid", "runtime"),
    "external_reference_storage": ("hybrid", "runtime"),
    "external_wrapper_storage": ("hybrid", "runtime"),
}

EXPECTED_SYMBOLS = {
    f"probe_{prefix}_{shape}"
    for shape, prefixes in SHAPES.items()
    for prefix in prefixes
}

SYMBOL_RE = re.compile(
    r"^[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\S\s+(probe_[a-z0-9_]+)$"
)


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
    missing = sorted(EXPECTED_SYMBOLS - sizes.keys())
    if missing:
        print(f"missing probe symbols: {', '.join(missing)}", file=sys.stderr)
        return 1

    expected_max = dict(EXPECTED_MAX)
    compiler_id = os.environ.get("DINGO_CXX_ID", "")
    is_clang = "clang" in compiler_id
    is_gcc = "g++" in compiler_id and not is_clang
    if is_clang:
        expected_max.update(CLANG_EXPECTED_MAX)
        machine = platform.machine().lower()
        if machine in ("aarch64", "arm64"):
            expected_max.update(CLANG_ARM64_EXPECTED_MAX)
    if "g++-13" in compiler_id:
        expected_max.update(GCC13_EXPECTED_MAX)
    if "g++-14" in compiler_id:
        expected_max.update(GCC14_EXPECTED_MAX)
    if "g++-15" in compiler_id:
        expected_max.update(GCC15_EXPECTED_MAX)
    machine = platform.machine().lower()
    if is_gcc and machine in ("aarch64", "arm64"):
        expected_max.update(GCC_ARM64_EXPECTED_MAX)

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
