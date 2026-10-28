#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate the exhaustive storage-form by request-shape table.

The model owns the request-shape catalog and the storage-form product. Every
form is checked against every shape through the real resolution kernel, and the
generated test writes the route/top-level table. Supported rows are not
hand-written: the library answers them, and the report makes the gaps visible.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from spec.gen.shared.runner import emit, render_template

from spec.model.grammar.catalog import ARRAY_STORAGE_FORMS, CONSUMING_SCOPES
from spec.model.semantics.storage_forms import iter_supported_forms
from spec.model.semantics.extended_shapes import extended_shapes
from spec.model.semantics.routes.rules import reason_text
from spec.gen.shared.validation import validate_catalog

# The base shape catalog is a leaf plus a chain of wrapper semantics. The
# path model adds the supported shapes that the bounded depth-2 catalog does
# not enumerate, so the compile-time table is the consumer of every supported
# path.
from spec.model.grammar.shapes import SHAPES as BASE_SHAPES, Shape

SHAPES = tuple(
    sorted({*BASE_SHAPES, *extended_shapes()}, key=lambda shape: shape.name)
)


ACCESS_CONSTRUCTORS = frozenset(
    {
        "lvalue_reference",
        "rvalue_reference",
        "pointer",
        "array_lvalue_reference",
        "array_pointer",
    }
)

def shape_family(shape: Shape) -> str:
    """The structural composition of a shape.

    The report's reason is rule-based now, but this helper is still useful in
    tests and for human-readable grouping.
    """
    names = [wrapper.name for wrapper in shape.wrappers]
    wrappers = [
        name
        for name in names
        if name
        in {
            "optional",
            "unique_ptr",
            "shared_ptr",
            "unique_array",
            "shared_array",
            "variant",
        }
    ]
    accesses = [
        name
        for name in names
        if name
        in {
            "lvalue_reference",
            "rvalue_reference",
            "pointer",
            "array_lvalue_reference",
            "array_pointer",
        }
    ]
    if len(wrappers) >= 2:
        return "nested wrapper"
    if wrappers:
        first = names.index(wrappers[0])
        inner = names[first + 1 :]
        if any(name in ACCESS_CONSTRUCTORS for name in inner):
            return "access form inside a wrapper"
        if "const" in inner:
            return "const-qualified wrapper element"
    if len(accesses) > 1:
        return "reference or pointer to an access form"
    if names and names[0] in {"array_lvalue_reference", "array_pointer"}:
        return "array view"
    return ""


def gap_reason(scope_name: str, form_name: str, shape: Shape) -> str:
    """Why a shape is not a published target of one storage form."""
    return reason_text(scope_name, form_name, shape)


def top_level_safe(scope_name: str, shape: Shape) -> bool:
    """Whether a matching route is safe to call on the container itself.

    A consuming scope produces its instance for one resolution. It can lend an
    lvalue reference to a longer-lived construction that owns the result, but a
    top-level borrow would outlive the transaction that produced it. Every other
    published route either returns the value, transfers ownership, or points at
    stable storage.
    """
    return not (scope_name in CONSUMING_SCOPES and shape.is_lvalue_reference)


def _constructor(form) -> str:
    return "int" if form.name in ARRAY_STORAGE_FORMS else form.cpp_type()


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope, form in iter_supported_forms():
        path = f"{scope.name}_{form.name}"
        rows.append(
            {
                "path": path,
                "scope": scope.name,
                "form": form.name,
                "type": form.cpp_type(),
                "constructor": _constructor(form),
                "scope_type": scope.cpp_type,
                "form_header": scope.header,
                "top_level": {
                    shape.name: top_level_safe(scope.name, shape)
                    for shape in SHAPES
                },
                "reasons": {
                    shape.name: gap_reason(scope.name, form.name, shape)
                    for shape in SHAPES
                },
            }
        )
    return rows


def validate() -> None:
    if not SHAPES:
        raise ValueError("shape catalog must not be empty")
    if len({shape.name for shape in SHAPES}) != len(SHAPES):
        raise ValueError("shape catalog contains duplicate names")
    rows = build_rows()
    if not rows:
        raise ValueError("type table must not be empty")
    if len({row["path"] for row in rows}) != len(rows):
        raise ValueError("type table contains duplicate forms")
    for row in rows:
        for shape in SHAPES:
            if not row["reasons"][shape.name]:
                raise ValueError(
                    f"form {row['form']} has no gap reason for {shape.name}"
                )


def render(output: Path) -> str:
    validate()
    return render_template("type_table.cpp.j2", rows=build_rows(), shapes=SHAPES)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    emit(arguments.out, render(arguments.out), check=arguments.check)


if __name__ == "__main__":
    main()
