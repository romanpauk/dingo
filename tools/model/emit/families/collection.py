#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Static collection registration and construction probes.

One file per collection flavor:

    resolution/static/global/collection/interface.cpp
    resolution/static/global/collection/keyed.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.hierarchy import CasePath


@dataclass(frozen=True)
class CollectionShard:
    kind: str
    path: str
    suite: str
    namespace: str
    case_path: str


def _shard(kind: str) -> CollectionShard:
    prefix = CasePath(
        phase="resolution",
        container="static",
        scope="global",
        storage="collection",
        category=kind,
    )
    return CollectionShard(
        kind=kind,
        path=prefix.file,
        suite=prefix.suite,
        namespace=prefix.suite,
        case_path=prefix.path,
    )


SHARDS = (_shard("interface"), _shard("keyed"))


def render(shard: CollectionShard) -> str:
    return render_template("collection_cells.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in SHARDS:
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
