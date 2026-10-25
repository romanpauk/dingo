#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate route-category probes for the built-in storage cells.

Each enabled scope/form cell is instantiated through the real storage type and
the real resolution lists. Value, object, and rvalue availability is compared
with the route ledger. Cells whose route shape is not modeled stay in the ledger
as tracked gaps instead of disappearing from the product.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from route_ledger import (
    category_access,
    category_kinds,
    category_retention,
    category_steps,
    category_targets,
    supported_categories,
    validate as validate_ledger,
)
from storage_cells import FORMS, SCOPES, validate_catalog


# Cells that publish no route at all and whose storage type cannot be named by
# an absence probe. The ledger still records them as unavailable; the generated
# probe set cannot instantiate their storage aliases.
ABSENCE_PROBE_GAPS = {
    ("shared", "array"): "raw dynamic array storage cannot be instantiated",
    ("unique", "array"): "raw dynamic array storage cannot be instantiated",
    ("external", "array"): "raw dynamic array storage cannot be instantiated",
    ("shared_cyclical", "array"): (
        "raw dynamic array storage cannot be instantiated"
    ),
    ("shared_cyclical", "unique_handle"): (
        "shared_cyclical cannot instantiate the form's storage type"
    ),
    ("shared_cyclical", "unique_array_handle"): (
        "shared_cyclical cannot instantiate the form's storage type"
    ),
    ("shared_cyclical", "shared_array_handle"): (
        "shared_cyclical cannot instantiate the form's storage type"
    ),
    ("shared_cyclical", "optional"): (
        "shared_cyclical cannot instantiate the form's storage type"
    ),
}


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope in SCOPES:
        for form in FORMS:
            categories = supported_categories(scope.name, form.name)
            if not categories:
                continue
            rows.append(
                {
                    "scope_name": scope.name,
                    "scope_type": scope.cpp_type,
                    "scope_header": scope.header,
                    "form_name": form.name,
                    "form_type": form.cpp_type,
                    "form_headers": form.headers,
                    "constructor_type": (
                        "int"
                        if form.name
                        in {
                            "array",
                            "bounded_array",
                            "unique_array_handle",
                            "shared_array_handle",
                        }
                        else form.cpp_type
                    ),
                    "value": "value" in categories,
                    "object": "object" in categories,
                    "rvalue": "rvalue" in categories,
                    "value_access": (
                        category_access(scope.name, form.name, "value")
                        if "value" in categories
                        else ""
                    ),
                    "object_access": (
                        category_access(scope.name, form.name, "object")
                        if "object" in categories
                        else ""
                    ),
                    "rvalue_access": (
                        category_access(scope.name, form.name, "rvalue")
                        if "rvalue" in categories
                        else ""
                    ),
                    "value_kinds": (
                        category_kinds(scope.name, form.name, "value")
                        if "value" in categories
                        else ()
                    ),
                    "object_kinds": (
                        category_kinds(scope.name, form.name, "object")
                        if "object" in categories
                        else ()
                    ),
                    "rvalue_kinds": (
                        category_kinds(scope.name, form.name, "rvalue")
                        if "rvalue" in categories
                        else ()
                    ),
                    "value_retention": (
                        category_retention(scope.name, form.name, "value")
                        if "value" in categories
                        else ()
                    ),
                    "value_steps": (
                        category_steps(scope.name, form.name, "value")
                        if "value" in categories
                        else ()
                    ),
                    "value_targets": (
                        category_targets(scope.name, form.name, "value")
                        if "value" in categories
                        else ()
                    ),
                    "object_retention": (
                        category_retention(scope.name, form.name, "object")
                        if "object" in categories
                        else ()
                    ),
                    "object_steps": (
                        category_steps(scope.name, form.name, "object")
                        if "object" in categories
                        else ()
                    ),
                    "object_targets": (
                        category_targets(scope.name, form.name, "object")
                        if "object" in categories
                        else ()
                    ),
                    "rvalue_retention": (
                        category_retention(scope.name, form.name, "rvalue")
                        if "rvalue" in categories
                        else ()
                    ),
                    "rvalue_steps": (
                        category_steps(scope.name, form.name, "rvalue")
                        if "rvalue" in categories
                        else ()
                    ),
                    "rvalue_targets": (
                        category_targets(scope.name, form.name, "rvalue")
                        if "rvalue" in categories
                        else ()
                    ),
                }
            )
    return rows


def validate() -> None:
    validate_ledger()
    rows = build_rows()
    if not rows:
        raise ValueError("route catalog must not be empty")
    keys = {(row["scope_name"], row["form_name"]) for row in rows}
    if len(keys) != len(rows):
        raise ValueError("route catalog contains duplicate cells")
    for scope in SCOPES:
        for form in FORMS:
            key = (scope.name, form.name)
            categories = supported_categories(*key)
            if categories and key in ABSENCE_PROBE_GAPS:
                raise ValueError(f"absence-probe gap is supported: {key}")
            if not categories and key not in ABSENCE_PROBE_GAPS:
                raise ValueError(f"unavailable cell has no absence probe: {key}")


def render(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("route_cells.cpp.j2")
    rows = build_rows()
    headers = sorted(
        {row["scope_header"] for row in rows}
        | {
            header
            for row in rows
            for header in row["form_headers"]
            if not str(header).startswith("dingo/")
        }
    )
    return template.render(rows=rows, headers=headers)


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
