#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Mixed co-residence classes.

A mixed container holds compile-time bindings and runtime registrations at the
same time. Two things can happen, and they are the whole family:

- a static binding resolves a dependency that only exists at runtime;
- the same type exists on both sides, and a singular request is ambiguous.

    resolution/mixed/global/<case>.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from spec.emit.common.runner import emit, render_template
from spec.emit.hierarchy import CasePath
from spec.emit.common.validation import validate_catalog


@dataclass(frozen=True)
class MixedCase:
    name: str
    path: str
    case_path: str
    suite: str
    namespace: str


def _case(name: str) -> MixedCase:
    case = CasePath(
        phase="resolution",
        container="mixed",
        scope="global",
        category=name,
    )
    return MixedCase(
        name=name,
        path=case.file,
        case_path=case.path,
        suite=case.suite,
        namespace=case.suite,
    )


SHARDS = (_case("external_dependency"), _case("singular_ambiguity"))


def render(shard: MixedCase) -> str:
    return render_template("mixed_cases.cpp.j2", shard=shard)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--include", type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    if arguments.include is not None:
        validate_catalog(arguments.include)
    for shard in SHARDS:
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
