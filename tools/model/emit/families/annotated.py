#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate runtime probes for the supported annotated paths.

The path model decides which annotated `(scope, storage, request)` cases are
supported. This family registers the annotated binding and resolves each case
twice; the identity check is derived from the inner request form.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from tools.model.grammar import cpp
from tools.model.grammar.catalog import FORM_BY_NAME
from tools.model.domain.cases import iter_cases, request_forms
from tools.model.emit.common.recipe import external_instance, registration
from tools.model.emit.common.runner import emit, render_template, trait_context
from tools.model.emit.common.validation import validate_catalog
from tools.model.semantics.support import classify
from tools.model.grammar.expr import Expr
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY, LeafTraits
from tools.model.grammar.storage import const_leaf, has_const

def _const_storage(case) -> bool:
    storage = case.storage_path.expr
    return const_leaf(storage) or (
        storage.op == "variant" and has_const(storage)
    )


def _storage_and_registration(
    case, traits: LeafTraits = COPYABLE
) -> tuple[str, str]:
    path = case.storage_path
    form = path.form
    const = _const_storage(case)
    interfaces = (
        cpp.annotated(cpp.leaf(traits.cpp), "tag_a").render(),
    )
    factory = (
        path.const_factory(traits.cpp)
        if const
        else path.factory(traits.cpp)
    )

    if form.kind == "value" and case.scope == "external":
        inner = case.request.inner
        storage = cpp.storage(inner.with_leaf(traits.cpp).cpp()).render()
        instance = f"{traits.cpp} external_instance"
        if inner.op == "pointer":
            argument = "std::addressof(external_instance)"
        elif inner.op == "lvalue_reference":
            argument = "external_instance"
        else:
            argument = "std::move(external_instance)"
        registration_line, _ = registration(
            form,
            case.scope,
            "runtime",
            interfaces=interfaces,
            external_argument=argument,
            storage=storage,
        )
        return instance, registration_line

    storage = path.storage(traits.cpp)
    if case.scope == "external":
        instance_spec = external_instance(
            form,
            traits.cpp,
            const=const,
            move=(
                not traits.copyable
                and path.expr.op != "lvalue_reference"
            ),
        )
        if instance_spec is None:
            raise ValueError(f"{form.name} has no external instance")
        registration_line, _ = registration(
            form,
            case.scope,
            "runtime",
            interfaces=interfaces,
            external_argument=instance_spec.argument,
            storage=storage,
        )
        return instance_spec.declaration, registration_line

    registration_line, _ = registration(
        form,
        case.scope,
        "runtime",
        interfaces=interfaces,
        storage=storage,
        factory=factory,
    )
    return "", registration_line


def _relation(inner: Expr) -> tuple[bool, str]:
    if inner.op == "lvalue_reference":
        return True, "&first == &second"
    if inner.op == "pointer":
        return False, "first == second"
    if inner.op in {
        "optional",
        "unique_ptr",
        "shared_ptr",
        "unique_array",
        "shared_array",
        "variant",
        "array",
        "array_reference",
        "array_pointer",
    }:
        return False, "true"
    return False, "first.id == second.id"


def build_rows(
    traits: LeafTraits = COPYABLE,
) -> list[dict[str, object]]:
    seen = set()
    rows = []
    for case in iter_cases(request_forms(), traits):
        if case.request.op != "annotated":
            continue
        if case.binding.kind != "annotated":
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
        instance, registration_line = _storage_and_registration(case, traits)
        bind_reference, relation = _relation(case.request.inner)
        rows.append(
            {
                "name": f"annotated_{case.scope}_{case.storage_name}_{len(rows)}",
                "scope": case.scope,
                "form": case.storage_name,
                "instance": instance,
                "registration": registration_line,
                "request": case.request.with_leaf(traits.cpp).render(),
                "relation": relation,
                "bind_reference": bind_reference,
                "coverage": (*key, traits.name),
            }
        )
    return rows


def validate(traits: LeafTraits = COPYABLE) -> None:
    rows = build_rows(traits)
    if not rows:
        raise ValueError("annotated catalog must not be empty")
    names = [row["name"] for row in rows]
    if len(names) != len(set(names)):
        raise ValueError("annotated catalog contains duplicate names")
    for row in rows:
        if not row["registration"]:
            raise ValueError(f"{row['name']} has no registration")
        if not row["request"]:
            raise ValueError(f"{row['name']} has no request")


def render(output: Path, traits: LeafTraits = COPYABLE) -> str:
    validate(traits)
    return render_template(
        "annotated_cells.cpp.j2",
        rows=build_rows(traits),
        **trait_context(traits, "annotated_cells"),
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--out-move-only", type=Path)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    for traits, out in (
        (COPYABLE, arguments.out),
        (MOVE_ONLY, arguments.out_move_only),
    ):
        if out is not None:
            emit(out, render(out, traits), check=arguments.check)


if __name__ == "__main__":
    main()
