#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate compile-time binding registration probes.

Each supported annotated, keyed, or interface case is rendered as a
`dingo::bindings<dingo::bind<...>>` type. Instantiating the type checks the
registration composition: scope, storage spelling, annotated/interface types,
and lookup key. It complements the runtime family, which checks the route.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

from tools.model.domain.cases import iter_cases, request_forms
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar import cpp
from tools.model.grammar.expr import Expr
from tools.model.semantics.support import classify
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY, LeafTraits

COPYABLE_LEAF = "implementation_type"
MOVE_ONLY_LEAF = "move_only_implementation_type"


def _leaf(traits: LeafTraits) -> str:
    return COPYABLE_LEAF if traits.copyable else MOVE_ONLY_LEAF
BINDINGS = frozenset({"annotated", "keyed", "interface"})


def _binding_type(case, traits: LeafTraits = COPYABLE) -> str:
    path = case.storage_path
    leaf = _leaf(traits)
    scope = cpp.template(
        "dingo::scope", cpp.text(f"dingo::{case.scope}")
    ).render()
    args = [
        cpp.text(scope),
        cpp.text(path.storage(leaf)),
    ]
    args.extend(case.binding.registration_interfaces(cpp.leaf(leaf)))
    key = case.binding.registration_key()
    if key is not None:
        args.append(key)
    return cpp.template(
        "dingo::bindings", cpp.template("dingo::bind", *args)
    ).render()


def _slug(expr: Expr) -> str:
    return re.sub(r"[^a-z0-9]+", "_", expr.render().lower()).strip("_")


def _rows_for(traits: LeafTraits) -> list[dict[str, object]]:
    rows = []
    seen = set()
    for case in iter_cases(request_forms(), traits):
        if case.binding.kind not in BINDINGS:
            continue
        verdict = classify(
            case.scope,
            case.storage_name,
            case.binding,
            case.request,
            traits,
        )
        if verdict.outcome != "supported":
            continue
        key = (case.scope, case.storage_name, case.request)
        if key in seen:
            continue
        seen.add(key)
        digest = hashlib.sha1(case.request.render().encode()).hexdigest()[:8]
        rows.append(
            {
                "name": (
                    f"{case.binding.kind}_{case.scope}_{case.storage_name}_"
                    f"{_slug(case.request)}_{digest}_{traits.name}"
                ),
                "binding": _binding_type(case, traits),
                "coverage": (*key, traits.name),
            }
        )
    return rows


def build_rows() -> list[dict[str, object]]:
    return [*_rows_for(COPYABLE), *_rows_for(MOVE_ONLY)]


def validate() -> None:
    rows = build_rows()
    if not rows:
        raise ValueError("binding matrix must not be empty")
    names = [row["name"] for row in rows]
    if len(names) != len(set(names)):
        raise ValueError("binding matrix contains duplicate names")
    for row in rows:
        if not row["binding"]:
            raise ValueError(f"{row['name']} has no binding type")


def render(output: Path) -> str:
    validate()
    return render_template("binding_matrix.cpp.j2", rows=build_rows())


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
