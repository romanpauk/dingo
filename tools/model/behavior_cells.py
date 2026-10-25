#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate the first behavior classes for the routing model.

The fast behavior tier checks identity, copy construction, and repeated
resolution for the plain shared and unique cells in both container front-ends.
The cells are representatives chosen by the same scope/form model; more classes
can be added without changing the runner shape.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from storage_cells import validate_catalog


SCOPES = ("shared", "unique", "external", "shared_cyclical")
CONTAINERS = ("runtime", "static")
RUNTIME_ONLY = {"external"}
DESTRUCTIONS = {
    ("shared", "runtime"): 3,
    ("shared", "static"): 3,
    ("unique", "runtime"): 4,
    ("unique", "static"): 4,
    ("external", "runtime"): 2,
    ("shared_cyclical", "runtime"): 1,
    ("shared_cyclical", "static"): 1,
}


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope in SCOPES:
        for container in CONTAINERS:
            if scope in RUNTIME_ONLY and container == "static":
                continue
            rows.append(
                {
                    "name": f"{scope}_{container}",
                    "kind": "plain",
                    "scope": scope,
                    "scope_type": f"dingo::scope<dingo::{scope}>",
                    "container": container,
                    "destructions": DESTRUCTIONS[(scope, container)],
                }
            )
    for scope, container, kind, destructions in (
        ("shared", "runtime", "shared_handle", 1),
        ("shared", "static", "shared_handle", 1),
        ("unique", "runtime", "unique_handle", 2),
        ("unique", "static", "unique_handle", 2),
        ("shared", "runtime", "shared_optional", 4),
        ("shared", "static", "shared_optional", 4),
        ("unique", "runtime", "unique_optional", 4),
        ("unique", "static", "unique_optional", 4),
        ("shared", "runtime", "shared_variant", 3),
        ("shared", "static", "shared_variant", 3),
        ("unique", "runtime", "unique_variant", 4),
        ("unique", "static", "unique_variant", 4),
        ("shared", "runtime", "shared_pointer", 3),
        ("shared", "static", "shared_pointer", 3),
        ("shared", "runtime", "shared_bounded_array", 2),
        ("shared", "static", "shared_bounded_array", 2),
        ("shared_cyclical", "runtime", "shared_cyclical_bounded_array", 2),
        ("shared_cyclical", "static", "shared_cyclical_bounded_array", 2),
    ):
        rows.append(
            {
                "name": f"{scope}_{kind}_{container}",
                "kind": kind,
                "scope": scope,
                "scope_type": f"dingo::scope<dingo::{scope}>",
                "container": container,
                "destructions": destructions,
            }
        )
    return rows


def validate() -> None:
    rows = build_rows()
    expected = len(SCOPES) * len(CONTAINERS) - len(RUNTIME_ONLY) + 18
    if len(rows) != expected:
        raise ValueError("behavior product is incomplete")
    if len({row["name"] for row in rows}) != len(rows):
        raise ValueError("behavior product contains duplicate cells")


def render(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("behavior_cells.cpp.j2")
    return template.render(rows=build_rows())


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
