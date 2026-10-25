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
generated test writes the supported/unsupported table. Supported rows are not
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


SHAPES = (
    Shape("value", "int"),
    Shape("lvalue_reference", "int &"),
    Shape("const_lvalue_reference", "const int &"),
    Shape("rvalue_reference", "int &&"),
    Shape("pointer", "int *"),
    Shape("const_pointer", "const int *"),
    Shape("pointer_pointer", "int **"),
    Shape("optional", "std::optional<int>"),
    Shape("optional_lvalue_reference", "std::optional<int> &"),
    Shape("optional_pointer", "std::optional<int> *"),
    Shape("unique_ptr", "std::unique_ptr<int>"),
    Shape("unique_ptr_lvalue_reference", "std::unique_ptr<int> &"),
    Shape("shared_ptr", "std::shared_ptr<int>"),
    Shape("shared_ptr_lvalue_reference", "std::shared_ptr<int> &"),
    Shape("array_lvalue_reference", "int (&)[2]"),
    Shape("array_pointer", "int (*)[2]"),
    Shape("unique_array_handle", "std::unique_ptr<int[]>"),
    Shape("unique_array_handle_lvalue_reference", "std::unique_ptr<int[]> &"),
    Shape("shared_array_handle", "std::shared_ptr<int[]>"),
    Shape("shared_array_handle_lvalue_reference", "std::shared_ptr<int[]> &"),
    Shape("optional_shared_ptr", "std::optional<std::shared_ptr<int>>"),
    Shape("shared_optional", "std::shared_ptr<std::optional<int>>"),
)

ARRAY_FORMS = frozenset(
    {"array", "bounded_array", "unique_array_handle", "shared_array_handle"}
)


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
