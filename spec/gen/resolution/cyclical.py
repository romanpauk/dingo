#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Cyclical publication classes.

A cycle is two slots that each need the other before either is complete, so the
container publishes each slot first and fills it afterwards. The mechanism's
degrees of freedom are the container mode and the storage representation of the
two nodes; the edge request form (`T &`) is the atom request space.

    resolution/<mode>/global/cyclical.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.gen.shared.runner import emit, render_template
from spec.gen.shared.hierarchy import CasePath

BIND = """dingo::bind<dingo::scope<dingo::shared_cyclical>,
                    dingo::storage<{storage}>>"""


@dataclass(frozen=True)
class CyclicalCase:
    name: str
    source: str
    container: str
    setup: str


def _shard_cases(mode: str) -> tuple[CyclicalCase, ...]:
    value_a = BIND.format(storage="node_a")
    value_b = BIND.format(storage="node_b")
    shared_a = BIND.format(storage="std::shared_ptr<node_a>")
    shared_b = BIND.format(storage="std::shared_ptr<node_b>")

    runtime = "dingo::container<> container;"
    static = "dingo::static_container<source> container;"

    runtime_case = (
        "container.register_type<dingo::scope<dingo::shared_cyclical>,\n"
        "                          dingo::storage<node_b>>();"
    )
    runtime_shared_b = (
        "container.register_type<dingo::scope<dingo::shared_cyclical>,\n"
        "                          dingo::storage<std::shared_ptr<node_b>>>();"
    )

    if mode == "runtime":
        return (
            CyclicalCase(
                name="value_pair",
                source=f"",
                container=runtime,
                setup=(
                    "container.register_type<dingo::scope<dingo::shared_cyclical>,\n"
                    "                          dingo::storage<node_a>>();\n  "
                    + runtime_case
                ),
            ),
            CyclicalCase(
                name="shared_pair",
                source="",
                container=runtime,
                setup=(
                    "container.register_type<dingo::scope<dingo::shared_cyclical>,\n"
                    "                          dingo::storage<std::shared_ptr<node_a>>>();\n  "
                    + runtime_shared_b
                ),
            ),
            CyclicalCase(
                name="mixed_storage",
                source="",
                container=runtime,
                setup=(
                    "container.register_type<dingo::scope<dingo::shared_cyclical>,\n"
                    "                          dingo::storage<node_a>>();\n  "
                    + runtime_shared_b
                ),
            ),
        )
    if mode == "static":
        return (
            CyclicalCase(
                name="value_pair",
                source=f"using source = dingo::bindings<\n    {value_a},\n    {value_b}>;",
                container=static,
                setup="",
            ),
            CyclicalCase(
                name="shared_pair",
                source=f"using source = dingo::bindings<\n    {shared_a},\n    {shared_b}>;",
                container=static,
                setup="",
            ),
        )



@dataclass(frozen=True)
class CyclicalShard:
    mode: str
    path: str
    case_path: str
    suite: str
    namespace: str
    cases: tuple[CyclicalCase, ...]


def build_shards() -> list[CyclicalShard]:
    shards = []
    for mode in ("runtime", "static"):
        case = CasePath(
            phase="resolution",
            container=mode,
            scope="global",
            category="cyclical",
        )
        shards.append(
            CyclicalShard(
                mode=mode,
                path=case.file,
                case_path=case.path,
                suite=case.suite,
                namespace=case.suite,
                cases=_shard_cases(mode),
            )
        )
    return shards


def render(shard: CyclicalShard) -> str:
    return render_template("cyclical_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
