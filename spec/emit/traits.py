#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Probe every storage form and request shape once per leaf trait.

Copyability is a property of the leaf, not of the shape. This matrix keeps the
shape catalog and the storage forms from `emit/matrix.py`, builds each form for
a copyable and a move-only leaf, and asks the real resolution kernel whether a
route is published for either.

The library's copyable answer is the baseline. The move-only answer must be a
subset of it, and it must be exactly the subset whose route does not copy the
leaf. That makes the copy boundary the tested fact: borrows, moves, and
owning-handle copies stay available, while a value request that copies the
leaf (or an inline wrapper that contains it) disappears.

This is the compile-time half of the trait model. The runtime families still
run the copyable leaf until each of them selects a trait-appropriate primary
route; `report/paths.py` owns the full path domain.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from spec.emit.common.runner import emit, render_template
from spec.emit.common.validation import validate_catalog
from spec.emit.matrix import SHAPES
from spec.grammar import cpp
from spec.grammar.catalog import ARRAY_STORAGE_FORMS
from spec.grammar.traits import COPYABLE, MOVE_ONLY
from spec.semantics.storage_forms import iter_supported_forms
from spec.semantics.routes.derivation import leaf_copy_required


def _slug(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def probe_shape(shape) -> bool:
    """Whether the compile-time lift trait is a trustworthy oracle.

    A const rvalue reference (``const T &&``) is a lift artefact: the compile
    trait answers for the inner value, while the runtime container cannot
    materialize a const rvalue of a move-only object. The path model rejects
    the copying form for a move-only leaf; the runtime families skip the shape
    instead of probing it. Every other shape is compared directly.
    """
    if shape.access != "rvalue_reference":
        return True
    inner = shape.inner
    return not (
        inner is not None
        and inner.outer is not None
        and inner.outer.name == "const"
    )


def build_rows() -> list[dict[str, object]]:
    rows = []
    shapes = [shape for shape in SHAPES if probe_shape(shape)]
    for scope, form in iter_supported_forms():
        rows.append(
            {
                "name": _slug(f"{scope.name}_{form.name}"),
                "scope": scope.name,
                "form": form.name,
                "scope_type": scope.cpp_type,
                "copyable_storage": form.cpp_type(COPYABLE.cpp),
                "move_only_storage": form.cpp_type(MOVE_ONLY.cpp),
                "copyable_constructor": (
                    COPYABLE.cpp
                    if form.name in ARRAY_STORAGE_FORMS
                    else form.cpp_type(COPYABLE.cpp)
                ),
                "move_only_constructor": (
                    MOVE_ONLY.cpp
                    if form.name in ARRAY_STORAGE_FORMS
                    else form.cpp_type(MOVE_ONLY.cpp)
                ),
                "shapes": [
                    {
                        "name": shape.name,
                        "copyable_type": cpp.rebase_leaf(
                            shape.cpp, COPYABLE.cpp
                        ).render(),
                        "move_only_type": cpp.rebase_leaf(
                            shape.cpp, MOVE_ONLY.cpp
                        ).render(),
                        "copies": leaf_copy_required(
                            scope.name, form.name, shape
                        ),
                    }
                    for shape in shapes
                ],
            }
        )
    return rows


def _check_unique(space: str, names) -> None:
    names = list(names)
    if len(names) != len(set(names)):
        raise ValueError(f"{space} contains duplicate names")


def validate() -> None:
    rows = build_rows()
    if not rows:
        raise ValueError("trait matrix must not be empty")
    _check_unique("trait matrix", (row["name"] for row in rows))
    if not SHAPES:
        raise ValueError("shape catalog must not be empty")
    _check_unique("shape catalog", (shape.name for shape in SHAPES))
    for row in rows:
        if not row["shapes"]:
            raise ValueError(f"form {row['name']} has no shapes")


def render(output: Path) -> str:
    validate()
    return render_template(
        "trait_matrix.cpp.j2",
        rows=build_rows(),
        copyable=COPYABLE,
        move_only=MOVE_ONLY,
    )


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
