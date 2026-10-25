#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Execute every supported storage-cell by request-shape row.

The type matrix says which shapes match. This suite resolves them for real:
every top-level shape is exercised against the same registration recipe as the
behavior cells, identity is checked for stable scopes, owned raw pointers are
released, and every block must finish with the lifetime balance intact.
Unsupported shapes are discarded by the compile-time support check, so the suite
follows the library rather than a hand-written list.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from jinja2 import Environment, FileSystemLoader, StrictUndefined

from behavior_cells import (
    BEHAVIOR_GAPS,
    EXTERNAL_INSTANCES,
    FORMS,
    SCOPES,
    _registration,
    supported_categories,
)
from storage_cells import validate_catalog
from type_matrix import CONSUMING_SCOPES, SHAPES, top_level_safe


ARRAY_FORMS = frozenset(
    {
        "array",
        "bounded_array",
        "unique_array_handle",
        "shared_array_handle",
    }
)

# A consuming scope produces its instance for one resolution. The route can
# lend a reference to a longer-lived construction that owns the result, but a
# top-level borrow outlives the transaction that produced it.
TOP_LEVEL_BORROW_GAP = (
    "a top-level lvalue borrow from a consuming scope outlives the resolution; "
    "the route serves longer-lived dependents"
)


def shape_gap(scope: str, shape) -> str:
    if not top_level_safe(scope, shape):
        return TOP_LEVEL_BORROW_GAP
    return ""


def delete_kind(scope: str, form: str, shape) -> str:
    """How a top-level raw pointer result is released by its new owner.

    A consuming scope transfers ownership of a raw pointer or array pointer to
    the caller. Every other scope hands out an address it keeps alive, so the
    caller must not release it.
    """
    if scope not in CONSUMING_SCOPES or not shape.is_pointer:
        return "none"
    if form == "bounded_array":
        return "array"
    if form == "pointer":
        return "single"
    return "none"


def _storage_type(form) -> str:
    prefix = "dingo::storage<"
    assert form.storage.startswith(prefix) and form.storage.endswith(">")
    return form.storage[len(prefix) : -1]


def _shape_type(shape) -> str:
    return re.sub(r"\bint\b", "tracked", shape.cpp_type)


def build_rows() -> list[dict[str, object]]:
    rows = []
    for scope_name in SCOPES:
        for form in FORMS:
            if not supported_categories(scope_name, form.name):
                continue
            # Cells the behavior model declares as gaps are not registrable in
            # the container front-end yet; the matrix still classifies them.
            if (scope_name, form.name) in BEHAVIOR_GAPS:
                continue
            instance = ""
            if scope_name == "external":
                external = EXTERNAL_INSTANCES.get(form.name)
                if external is None:
                    continue
                instance = external[0]
            registration, _ = _registration(
                form.storage, form.factory, scope_name, "runtime"
            )
            rows.append(
                {
                    "name": f"{scope_name}_{form.name}",
                    "scope": scope_name,
                    "form": form.name,
                    "storage_type": _storage_type(form),
                    "constructor": (
                        "tracked"
                        if form.name in ARRAY_FORMS
                        else _storage_type(form)
                    ),
                    "registration": registration,
                    "instance": instance,
                    "stable": scope_name not in CONSUMING_SCOPES,
                    "shapes": [
                        {
                            "name": shape.name,
                            "type": _shape_type(shape),
                            "gap": shape_gap(scope_name, shape),
                            "delete": delete_kind(
                                scope_name, form.name, shape
                            ),
                        }
                        for shape in SHAPES
                    ],
                }
            )
    return rows


def validate() -> None:
    rows = build_rows()
    if not rows:
        raise ValueError("type behavior catalog must not be empty")
    if len({row["name"] for row in rows}) != len(rows):
        raise ValueError("type behavior catalog contains duplicate cells")
    if not SHAPES:
        raise ValueError("shape catalog must not be empty")
    for row in rows:
        if row["scope"] == "external" and not row["instance"]:
            raise ValueError(f"external cell {row['name']} needs an instance")
        executable = [shape for shape in row["shapes"] if not shape["gap"]]
        if not executable:
            raise ValueError(f"cell {row['name']} has no executable shape")


def render(output: Path) -> str:
    validate()
    environment = Environment(
        loader=FileSystemLoader(Path(__file__).parent / "templates"),
        undefined=StrictUndefined,
        autoescape=False,
        keep_trailing_newline=True,
    )
    template = environment.get_template("type_behavior.cpp.j2")
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
