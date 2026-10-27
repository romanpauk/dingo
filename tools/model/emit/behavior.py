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
released, and every block must finish with the lifetime balance intact. The
catalog is emitted twice: once through the runtime container and once through
`static_container` for every statically bindable cell. Unsupported shapes are
discarded by the compile-time support check, so the suite follows the library
rather than a hand-written list.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from tools.model.emit.common.runner import emit, render_template

from tools.model.emit.families.behavior import BEHAVIOR_GAPS, FORMS, SCOPES
from tools.model.grammar.catalog import ARRAY_STORAGE_FORMS, RUNTIME_ONLY_SCOPES
from tools.model.semantics.cells import iter_supported_cells
from tools.model.emit.common.recipe import external_instance, registration
from tools.model.emit.common.validation import validate_catalog
from tools.model.emit.matrix import SHAPES, CONSUMING_SCOPES, top_level_safe


CONTAINERS = ("runtime", "static")

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
    return form.cpp("tracked").render()


def _shape_type(shape) -> str:
    return shape.with_leaf("tracked")


def build_rows(
    containers: tuple[str, ...] = CONTAINERS,
) -> list[dict[str, object]]:
    rows = []
    for scope, form in iter_supported_cells():
        scope_name = scope.name
        # Cells the behavior model declares as gaps are not registrable in
        # the container front-end yet; the matrix still classifies them.
        if (scope_name, form.name) in BEHAVIOR_GAPS:
            continue
        instance = None
        if scope_name == "external":
            instance = external_instance(form)
            if instance is None:
                continue
        for container in containers:
            if scope_name in RUNTIME_ONLY_SCOPES and container == "static":
                continue
            registration_line, static_source = registration(
                form,
                scope_name,
                container,
                external_argument=(
                    instance.argument if instance is not None else ""
                ),
            )
            rows.append(
                {
                    "name": f"{scope_name}_{form.name}_{container}",
                    "scope": scope_name,
                    "form": form.name,
                    "container": container,
                    "storage_type": _storage_type(form),
                    "constructor": (
                        "tracked"
                        if form.name in ARRAY_STORAGE_FORMS
                        else _storage_type(form)
                    ),
                    "registration": registration_line,
                    "static_source": static_source,
                    "instance": instance.declaration if instance else "",
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


def validate(containers: tuple[str, ...] = CONTAINERS) -> None:
    rows = build_rows(containers)
    if not rows:
        raise ValueError("type behavior catalog must not be empty")
    if len({row["name"] for row in rows}) != len(rows):
        raise ValueError("type behavior catalog contains duplicate cells")
    if not SHAPES:
        raise ValueError("shape catalog must not be empty")
    for row in rows:
        if row["container"] == "static" and not row["static_source"]:
            raise ValueError(f"static cell {row['name']} needs a source")
        if row["container"] == "runtime" and not row["registration"]:
            raise ValueError(f"runtime cell {row['name']} needs a registration")
        if row["scope"] == "external" and not row["instance"]:
            raise ValueError(f"external cell {row['name']} needs an instance")
        if row["scope"] in RUNTIME_ONLY_SCOPES and row["container"] == "static":
            raise ValueError(f"cell {row['name']} cannot be static")
        executable = [shape for shape in row["shapes"] if not shape["gap"]]
        if not executable:
            raise ValueError(f"cell {row['name']} has no executable shape")


def render(
    rows: list[dict[str, object]],
) -> str:
    return render_template("type_behavior.cpp.j2", rows=rows)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--out-static", type=Path)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    validate(("runtime",))
    runtime_content = render(build_rows(("runtime",)))
    static_content = ""
    if arguments.out_static is not None:
        validate(("static",))
        static_content = render(build_rows(("static",)))

    outputs = [(arguments.out, runtime_content)]
    if arguments.out_static is not None:
        outputs.append((arguments.out_static, static_content))

    for path, content in outputs:
        emit(path, content, check=arguments.check)


if __name__ == "__main__":
    main()
