#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Render the consumer claim files from the resolve manifest.

A resolve row is a capability: one request type against one registered cell.
``resolve`` is the first claim about that row. ``construct`` and ``invoke`` are
claims about the same row through a different entry point, so they are rendered
from the same manifest instead of enumerating storage again:

    resolution/runtime/<scope>/<storage>/construct.cpp
    resolution/runtime/<scope>/<storage>/invoke.cpp

The case path keeps the cell and only swaps the claim level:

    resolution/runtime/<scope>/<storage>/<claim>/<traits>/<binding>/<request>

Rows that cannot even form the composition are excluded here and recorded in
``NON_COMPOSABLE``; the claim files must only contain rows that compile.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template

CONSUMERS = ("construct", "invoke")

# Rows whose request type cannot be turned into an operator argument. These are
# compile-time gaps: no runtime assertion can express them. They are pinned
# explicitly and sorted, so a new gap cannot be absorbed silently by a rule.
#
# Cause: the constructor argument adaptor is ambiguous when the constructor
# parameter is ``std::optional<...>``. ``resolve`` and ``invoke`` handle the
# same rows.
_CONSTRUCT_OPTIONAL_PARAMETER = (
    "constructor parameter cannot be converted from the resolved optional value"
)

# Resolving an rvalue reference into an element of a unique bounded array
# transfers the element and the array still destroys it, so the probe crashes
# instead of throwing. Pinned until the ownership bug is fixed.
_CONSTRUCT_BOUNDED_ARRAY_RVALUE = (
    "rvalue reference into a unique bounded array is destroyed twice"
)

COMPILE_GAPS: dict[str, dict[str, str]] = {
    "construct": {
    "resolution/runtime/external/alternative/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/alternative/resolve/copyable/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/alternative/resolve/copyable/concrete/optional_variant_variant_t_long_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/optional/resolve/copyable/concrete/optional_optional_optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/optional/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/optional/resolve/copyable/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/optional_optional_std_optional_const_t/resolve/copyable/concrete/optional_optional_optional_const_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/optional_optional_std_optional_const_t/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/plain/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/plain_lvalue_reference_t/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/pointer/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/pointer_const_const_t_const/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/pointer_const_t_const/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/pointer_pointer_const_t/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_array_handle/resolve/copyable/concrete/optional_shared_array_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_array_handle/resolve/copyable/concrete/optional_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_array_handle/resolve/move_only/concrete/optional_shared_array_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_array_handle/resolve/move_only/concrete/optional_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle/resolve/copyable/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle/resolve/copyable/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle/resolve/move_only/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle/resolve/move_only/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle_shared_ptr_std_shared_ptr_const_t/resolve/copyable/concrete/optional_shared_ptr_const_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle_shared_ptr_std_shared_ptr_const_t/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/external/shared_handle_shared_ptr_std_shared_ptr_const_t/resolve/move_only/concrete/optional_shared_ptr_const_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/alternative/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/alternative/resolve/copyable/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/alternative/resolve/copyable/concrete/optional_variant_variant_t_long_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/optional/resolve/copyable/concrete/optional_optional_optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/optional/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/optional/resolve/copyable/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/optional_optional_std_optional_const_t/resolve/copyable/concrete/optional_optional_optional_const_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/optional_optional_std_optional_const_t/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/plain/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/pointer/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/pointer_const_const_t_const/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/pointer_const_t_const/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/pointer_pointer_const_t/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_array_handle/resolve/copyable/concrete/optional_shared_array_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_array_handle/resolve/copyable/concrete/optional_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_array_handle/resolve/move_only/concrete/optional_shared_array_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_array_handle/resolve/move_only/concrete/optional_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle/resolve/copyable/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle/resolve/copyable/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle/resolve/move_only/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle/resolve/move_only/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle_shared_ptr_std_shared_ptr_const_t/resolve/copyable/concrete/optional_shared_ptr_const_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle_shared_ptr_std_shared_ptr_const_t/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared/shared_handle_shared_ptr_std_shared_ptr_const_t/resolve/move_only/concrete/optional_shared_ptr_const_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared_cyclical/shared_handle/resolve/copyable/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared_cyclical/shared_handle/resolve/copyable/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared_cyclical/shared_handle/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared_cyclical/shared_handle/resolve/move_only/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/shared_cyclical/shared_handle/resolve/move_only/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/alternative/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/alternative/resolve/copyable/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/alternative/resolve/copyable/concrete/optional_variant_variant_t_long_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/alternative/resolve/move_only/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/alternative/resolve/move_only/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/alternative/resolve/move_only/concrete/optional_variant_variant_t_long_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/copyable/concrete/optional_optional_optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/copyable/concrete/optional_t_rref": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/copyable/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/move_only/concrete/optional_optional_optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/move_only/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/move_only/concrete/optional_t_rref": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/optional/resolve/move_only/concrete/optional_variant_t_long": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/plain/resolve/copyable/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/plain/resolve/move_only/concrete/optional_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_array_handle/resolve/copyable/concrete/optional_shared_array_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_array_handle/resolve/copyable/concrete/optional_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_array_handle/resolve/move_only/concrete/optional_shared_array_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_array_handle/resolve/move_only/concrete/optional_shared_array_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_handle/resolve/copyable/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_handle/resolve/copyable/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_handle/resolve/move_only/concrete/optional_shared_ptr_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    "resolution/runtime/unique/shared_handle/resolve/move_only/concrete/optional_shared_ptr_t": _CONSTRUCT_OPTIONAL_PARAMETER,
    },
    "invoke": {},
}

RUNTIME_PINS: dict[str, dict[str, str]] = {
    "construct": {
        "resolution/runtime/unique/bounded_array_array_t_2/resolve/copyable/concrete/t_rref": _CONSTRUCT_BOUNDED_ARRAY_RVALUE,
        "resolution/runtime/unique/bounded_array_array_t_3/resolve/copyable/concrete/t_rref": _CONSTRUCT_BOUNDED_ARRAY_RVALUE,
    },
    "invoke": {},
}

# Rows the claim files must not contain, by claim.
NON_COMPOSABLE: dict[str, dict[str, str]] = {
    consumer: {**COMPILE_GAPS[consumer], **RUNTIME_PINS[consumer]}
    for consumer in CONSUMERS
}


@dataclass(frozen=True)
class ConsumerFile:
    consumer: str
    scope: str
    storage: str
    path: str
    suite: str
    cases: tuple[dict[str, object], ...]


def _claim_path(case: dict[str, object], consumer: str) -> str:
    path = str(case["path"])
    marker = "/resolve/"
    if marker not in path:
        raise SystemExit(f"unexpected case path: {path}")
    return path.replace(marker, f"/{consumer}/", 1)


def build_files(manifest: dict[str, object], consumer: str) -> tuple[ConsumerFile, ...]:
    excluded = NON_COMPOSABLE[consumer]
    files: list[ConsumerFile] = []
    for shard in manifest["shards"]:  # type: ignore[index]
        cases = []
        for case in shard["cases"]:  # type: ignore[index]
            if str(case["path"]) in excluded:
                continue
            cases.append({**case, "claim_path": _claim_path(case, consumer)})
        if not cases:
            continue
        scope = str(shard["scope"])
        storage = str(shard["storage"])
        files.append(
            ConsumerFile(
                consumer=consumer,
                scope=scope,
                storage=storage,
                path=f"resolution/runtime/{scope}/{storage}/{consumer}.cpp",
                suite=f"resolution_runtime_{scope}_{storage}_{consumer}",
                cases=tuple(cases),
            )
        )
    return tuple(files)


def render(file: ConsumerFile) -> str:
    return render_template("consumer_cells.cpp.j2", file=file)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--consumer", choices=CONSUMERS, required=True)
    parser.add_argument(
        "--shard",
        action="append",
        help="only emit the named cell (repeatable, matches <scope>/<storage>)",
    )
    parser.add_argument(
        "--list-excluded",
        action="store_true",
        help="print the declared non-composable rows and exit",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    manifest = json.loads(arguments.manifest.read_text(encoding="utf-8"))
    if arguments.list_excluded:
        for path in sorted(NON_COMPOSABLE[arguments.consumer]):
            print(path)
        return
    files = build_files(manifest, arguments.consumer)
    if arguments.shard:
        wanted = set(arguments.shard)
        files = tuple(
            file for file in files if f"{file.scope}/{file.storage}" in wanted
        )
        if not files:
            raise SystemExit(
                f"unknown shards: {', '.join(sorted(wanted))}"
            )
    for file in files:
        emit(arguments.out_dir / file.path, render(file), check=arguments.check)


if __name__ == "__main__":
    main()
