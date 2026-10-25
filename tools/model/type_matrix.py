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
from dataclasses import dataclass
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from route_ledger import supported_categories
from storage_cells import FORMS, SCOPES, exposure, validate_catalog


@dataclass(frozen=True)
class Shape:
    name: str
    cpp_type: str
    depth: int = 0
    is_reference: bool = False
    is_lvalue_reference: bool = False
    is_pointer: bool = False
    is_array: bool = False
    is_const: bool = False


BASE = Shape("value", "int")


@dataclass(frozen=True)
class Constructor:
    name: str
    render: str
    is_reference: bool = False
    is_lvalue_reference: bool = False
    is_pointer: bool = False
    is_array: bool = False
    is_const: bool = False

    def apply(self, inner: Shape) -> Shape:
        return Shape(
            name=f"{self.name}_{inner.name}",
            cpp_type=self.render.format(inner=inner.cpp_type),
            depth=inner.depth + 1,
            is_reference=self.is_reference,
            is_lvalue_reference=self.is_lvalue_reference,
            is_pointer=self.is_pointer,
            is_array=self.is_array,
            is_const=self.is_const,
        )

    def applicable(self, inner: Shape) -> bool:
        if self.is_const:
            # const applies to the object, not to pointers or references; the
            # pointer-to-const shape comes from pointer(const(T)).
            return not inner.is_reference and not inner.is_const
        if self.is_reference:
            return not inner.is_reference and not inner.is_array
        if self.is_array:
            return not inner.is_reference and not inner.is_array
        if self.name in {"pointer", "array_pointer"}:
            return not inner.is_reference and not inner.is_array
        if self.name in {
            "optional",
            "unique_ptr",
            "shared_ptr",
            "unique_array",
            "shared_array",
        }:
            return not inner.is_reference and not inner.is_array
        return False


CONSTRUCTORS = (
    Constructor("const", "const {inner}", is_const=True),
    Constructor(
        "lvalue_reference", "{inner} &", is_reference=True,
        is_lvalue_reference=True,
    ),
    Constructor("rvalue_reference", "{inner} &&", is_reference=True),
    Constructor("pointer", "{inner} *", is_pointer=True),
    Constructor("optional", "std::optional<{inner}>"),
    Constructor("unique_ptr", "std::unique_ptr<{inner}>"),
    Constructor("shared_ptr", "std::shared_ptr<{inner}>"),
    Constructor("unique_array", "std::unique_ptr<{inner}[]>"),
    Constructor("shared_array", "std::shared_ptr<{inner}[]>"),
    Constructor(
        "array_lvalue_reference", "{inner} (&)[2]", is_reference=True,
        is_lvalue_reference=True, is_array=True,
    ),
    Constructor("array_pointer", "{inner} (*)[2]", is_pointer=True,
                is_array=True),
)

SHAPE_DEPTH = 2


def enumerate_shapes(depth: int = SHAPE_DEPTH) -> tuple[Shape, ...]:
    shapes = {BASE.name: BASE}
    frontier = [BASE]
    for _ in range(depth):
        next_frontier = []
        for inner in frontier:
            for constructor in CONSTRUCTORS:
                if not constructor.applicable(inner):
                    continue
                shape = constructor.apply(inner)
                if shape.name in shapes:
                    continue
                shapes[shape.name] = shape
                next_frontier.append(shape)
        frontier = next_frontier
    # A bare array or a top-level const is not a request type. Reference and
    # pointer forms of arrays are included.
    return tuple(
        sorted(
            (
                shape
                for shape in shapes.values()
                if shape.depth == 0 or not shape.is_const
            ),
            key=lambda shape: shape.name,
        )
    )


SHAPES = enumerate_shapes()


ARRAY_FORMS = frozenset(
    {"array", "bounded_array", "unique_array_handle", "shared_array_handle"}
)

CONSUMING_SCOPES = frozenset(scope.name for scope in SCOPES if scope.consumes)


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
