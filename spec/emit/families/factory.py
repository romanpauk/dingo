#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Explicit factory registration probes.

Two registration forms are covered first:

    registration/runtime/shared/plain/factory/function.cpp
    registration/runtime/shared/plain/factory/callable.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.emit.common.runner import emit, render_template
from spec.emit.hierarchy import CasePath


@dataclass(frozen=True)
class FactoryShard:
    kind: str
    path: str
    suite: str
    namespace: str
    case_path: str


SHARDS = tuple(
    FactoryShard(
        kind=kind,
        path=CasePath(
            phase="registration",
            container="runtime",
            scope="shared",
            storage="plain",
            category=f"factory/{kind}",
        ).file,
        suite=CasePath(
            phase="registration",
            container="runtime",
            scope="shared",
            storage="plain",
            category=f"factory/{kind}",
        ).suite,
        namespace=CasePath(
            phase="registration",
            container="runtime",
            scope="shared",
            storage="plain",
            category=f"factory/{kind}",
        ).suite,
        case_path=f"registration/runtime/shared/plain/factory/{kind}",
    )
    for kind in (
        "function",
        "callable",
        "constructor",
        "detected",
        "typedef",
        "overloaded_callable",
    )
)


def render(shard: FactoryShard) -> str:
    return render_template("factory_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    for shard in SHARDS:
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
