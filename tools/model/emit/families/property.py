#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate the wrapper-property probe.

Every wrapper declares its semantics once in the Python model. This generator
turns those declarations into static assertions against the C++ `type_traits`
flags, so a built-in or user wrapper cannot silently change its owning,
sharing, nullable, borrowable, array, variant, copy-out, or take behavior.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.hierarchy import CasePath
from tools.model.grammar.shapes import BASE, CUSTOM_WRAPPERS, WRAPPERS, WrapperSemantics


@dataclass(frozen=True)
class PropertyRow:
    name: str
    cpp_type: str
    kind: str
    is_owning: bool
    is_sharing: bool
    is_nullable: bool
    is_borrowable: bool
    is_array: bool
    is_variant: bool
    can_copy_out: bool
    can_take: bool


def _properties(
    wrapper: WrapperSemantics,
) -> tuple[bool, bool, bool, bool, bool, bool, bool, bool]:
    owning = wrapper.has("owning")
    sharing = wrapper.has("sharing")
    nullable = wrapper.has("nullable")
    borrowable = wrapper.has("borrowable")
    is_array = wrapper.has("array_handle")
    variant = wrapper.kind == "variant"
    return (
        owning,
        sharing,
        nullable,
        borrowable,
        is_array,
        variant,
        borrowable and not is_array,
        wrapper.has("sole_owner") or sharing,
    )


def build_rows() -> list[PropertyRow]:
    rows = []
    for wrapper in (*WRAPPERS, *CUSTOM_WRAPPERS):
        if wrapper.kind not in {
            "value",
            "handle",
            "array_handle",
            "variant",
            "pointer",
        }:
            continue
        if not wrapper.applicable(BASE):
            continue
        shape = BASE.apply(wrapper)
        cpp_type = shape.cpp_type
        (
            owning,
            sharing,
            nullable,
            borrowable,
            is_array,
            variant,
            copy_out,
            take,
        ) = _properties(wrapper)
        rows.append(
            PropertyRow(
                name=f"property_{wrapper.name}",
                cpp_type=cpp_type,
                kind=wrapper.kind,
                is_owning=owning,
                is_sharing=sharing,
                is_nullable=nullable,
                is_borrowable=borrowable,
                is_array=is_array,
                is_variant=variant,
                can_copy_out=copy_out,
                can_take=take,
            )
        )
    return rows


def validate() -> None:
    rows = build_rows()
    if not rows:
        raise ValueError("property catalog must not be empty")
    names = [row.name for row in rows]
    if len(names) != len(set(names)):
        raise ValueError("property catalog contains duplicate names")
    for row in rows:
        if row.can_take and not row.is_owning:
            raise ValueError(f"{row.name} takes without owning")
        if row.can_copy_out and not row.is_borrowable:
            raise ValueError(f"{row.name} copies out without borrowing")


@dataclass(frozen=True)
class PropertyShard:
    name: str
    path: str
    suite: str
    case_path: str
    row: PropertyRow


def build_shards() -> list[PropertyShard]:
    shards = []
    for row in build_rows():
        name = row.name.removeprefix("property_")
        path = CasePath(
            phase="detection",
            container="",
            scope="global",
            category=f"wrapper/{name}",
            traits="",
            binding="",
            request="properties",
        )
        shards.append(
            PropertyShard(
                name=name,
                path=path.file,
                suite=path.suite,
                case_path=path.path,
                row=row,
            )
        )
    return shards


def render(shard: PropertyShard) -> str:
    return render_template(
        "property_cells.cpp.j2",
        shard=shard,
    )


def manifest() -> str:
    payload = {
        "shards": [
            {
                "name": shard.name,
                "path": shard.path,
                "suite": shard.suite,
                "case": shard.case_path,
            }
            for shard in build_shards()
        ]
    }
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    validate()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)
    if arguments.manifest is not None:
        emit(arguments.manifest, manifest(), check=arguments.check)


if __name__ == "__main__":
    main()
