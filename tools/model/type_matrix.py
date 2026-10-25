#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate the exhaustive storage-cell by request-shape matrix.

The model owns the request-shape catalog and the storage-cell product. Every
cell is checked against every shape through the real resolution kernel, and the
generated test writes the route/top-level table. Supported rows are not
hand-written: the library answers them, and the report makes the gaps visible.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from route_ledger import supported_categories
from route_rules import reason_text
from storage_cells import FORMS, SCOPES, exposure, validate_catalog

# The shape catalog is a leaf plus a chain of wrapper semantics. The wrapper
# model owns applicability and C++ spelling; the route rules own the reason a
# request is not derived.
from wrapper_semantics import SHAPES, Shape


ARRAY_FORMS = frozenset(
    {"array", "bounded_array", "unique_array_handle", "shared_array_handle"}
)

CONSUMING_SCOPES = frozenset(scope.name for scope in SCOPES if scope.consumes)

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
    """Why a shape is not a published target of one cell."""
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
    return "int" if form.name in ARRAY_FORMS else form.cpp_type


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope in SCOPES:
        for form in FORMS:
            if exposure(scope, form) == 0:
                continue
            # A cell the ledger declares unavailable has no instantiable
            # storage, so it cannot enter the shape matrix.
            if not supported_categories(scope.name, form.name):
                continue
            cell = f"{scope.name}_{form.name}"
            rows.append(
                {
                    "cell": cell,
                    "scope": scope.name,
                    "form": form.name,
                    "type": form.cpp_type,
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
        raise ValueError("type matrix must not be empty")
    if len({row["cell"] for row in rows}) != len(rows):
        raise ValueError("type matrix contains duplicate cells")
    for row in rows:
        for shape in SHAPES:
            if not row["reasons"][shape.name]:
                raise ValueError(
                    f"cell {row['cell']} has no gap reason for {shape.name}"
                )


def render(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("type_matrix.cpp.j2")
    return template.render(rows=build_rows(), shapes=SHAPES)


def write_text_if_changed(path: Path, content: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


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
