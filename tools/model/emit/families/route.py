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

from tools.model.grammar.catalog import FORMS, SCOPES
from tools.model.semantics.cells import iter_supported_cells
from tools.model.semantics.gaps import cell_gaps, validate as validate_gaps
from tools.model.semantics.routes.ledger import (
    category_access,
    category_candidates,
    supported_categories,
    validate as validate_ledger,
)
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog


# Cells that publish no route at all and whose storage type cannot be named by
# an absence probe. The ledger still records them as unavailable; the generated
# probe set cannot instantiate their storage aliases.
ABSENCE_PROBE_GAPS = cell_gaps("absence")


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope, form in iter_supported_cells():
        categories = supported_categories(scope.name, form.name)
        rows.append(
            {
                "scope_name": scope.name,
                "scope_type": scope.cpp_type,
                "scope_header": scope.header,
                "form_name": form.name,
                "form_type": form.cpp_type(),
                "form_headers": form.headers,
                "constructor_type": (
                    "int"
                    if form.kind in {"array", "array_handle"}
                    else form.cpp_type()
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
                "value_candidates": (
                    category_candidates(scope.name, form.name, "value")
                    if "value" in categories
                    else ()
                ),
                "object_candidates": (
                    category_candidates(scope.name, form.name, "object")
                    if "object" in categories
                    else ()
                ),
                "rvalue_candidates": (
                    category_candidates(scope.name, form.name, "rvalue")
                    if "rvalue" in categories
                    else ()
                ),
            }
        )
    return rows


def validate() -> None:
    validate_gaps()
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
    return render_template("route_cells.cpp.j2", rows=rows, headers=headers)


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
