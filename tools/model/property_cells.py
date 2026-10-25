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
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from wrapper_semantics import BASE, CUSTOM_WRAPPERS, WRAPPERS, WrapperSemantics


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
        cpp_type = (
            f"dingo::{shape.cpp_type}"
            if wrapper in CUSTOM_WRAPPERS
            else shape.cpp_type
        )
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


def render(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("property_cells.cpp.j2")
    return template.render(rows=build_rows())


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    content = render(arguments.out)
    if arguments.check:
        if not arguments.out.exists() or arguments.out.read_text(
            encoding="utf-8"
        ) != content:
            raise SystemExit(f"{arguments.out} is out of date")
        return
    write_text_if_changed(arguments.out, content)


if __name__ == "__main__":
    main()
