#!/usr/bin/env python3

"""Checks that a resolution from the static container compiles to no more
code than the same resolution from the runtime container. The sizes of the
probes themselves are tracked by the footprint check, against its last run."""

import re
import sys
from pathlib import Path


# The bytes a static probe may exceed its runtime probe by. The static
# interface handle rolls the container back when the operation fails: a
# cleanup that resets what it resolved, which keeps clang from folding the
# kept wrappers it destroys. Clang 18 compiles the runtime probe smaller than
# the static one by up to 0x3c bytes on arm64.
RUNTIME_PATH_MARGIN = {
    "probe_static_resolution_interface_handle": 0x40,
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
)

SHAPES = {
    "consumer_read": (
        "static_resolution",
        "runtime_resolution",
    ),
    "shared_config": (
        "static_resolution",
        "runtime_resolution",
    ),
    "shared_value_config": (
        "static_resolution",
        "runtime_resolution",
    ),
    "shared_reference_config": (
        "static_resolution",
        "runtime_resolution",
    ),
    "optional_config": (
        "static_resolution",
        "runtime_resolution",
    ),
    "unique_value_config": (
        "static_resolution",
        "runtime_resolution",
    ),
    "unique_rvalue_config": (
        "static_resolution",
        "runtime_resolution",
    ),
    "unique_wrapper_config": (
        "static_resolution",
        "runtime_resolution",
    ),
    "interface_handle": (
        "static_resolution",
        "runtime_resolution",
    ),
    "collection_sum": (
        "static_resolution",
        "runtime_resolution",
    ),
    "external_value_storage": (
        "runtime_resolution",
    ),
    "external_reference_storage": (
        "runtime_resolution",
    ),
    "external_wrapper_storage": (
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

    failed = False
    for faster_symbol, slower_symbol in EXPECTED_FASTER_THAN_RUNTIME:
        margin = RUNTIME_PATH_MARGIN.get(faster_symbol, 0)
        if sizes[faster_symbol] > sizes[slower_symbol] + margin:
            print(
                f"{faster_symbol} regressed past runtime path: "
                f"0x{sizes[faster_symbol]:x} > 0x{sizes[slower_symbol]:x}"
                + (f" + 0x{margin:x}" if margin else ""),
                file=sys.stderr,
            )
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
