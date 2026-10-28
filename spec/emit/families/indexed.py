#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Indexed runtime lookup probes.

    resolution/runtime/indexed/global/<key>/resolve.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.emit.common.runner import emit, render_template
from spec.emit.hierarchy import CasePath


@dataclass(frozen=True)
class IndexedShard:
    key: str
    key_type: str
    traits: str
    first: str
    second: str
    path: str
    suite: str
    namespace: str
    case_path: str


def _shard(
    key: str, key_type: str, traits: str, first: str, second: str
) -> IndexedShard:
    prefix = CasePath(
        phase="resolution",
        container="runtime",
        configuration="indexed",
        scope="global",
        storage=key,
        category="resolve",
    )
    return IndexedShard(
        key=key,
        key_type=key_type,
        traits=traits,
        first=first,
        second=second,
        path=prefix.file,
        suite=prefix.suite,
        namespace=prefix.suite,
        case_path=prefix.path,
    )


SHARDS = (
    _shard(
        "size_t",
        "std::size_t",
        "indexed_container_traits",
        "std::size_t{0}",
        "std::size_t{1}",
    ),
    _shard(
        "int",
        "int",
        "indexed_int_container_traits",
        "int{0}",
        "int{1}",
    ),
    _shard(
        "string",
        "std::string",
        "indexed_string_container_traits",
        'std::string{"a"}',
        'std::string{"b"}',
    ),
)


def render(shard: IndexedShard) -> str:
    return render_template("indexed_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in SHARDS:
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
