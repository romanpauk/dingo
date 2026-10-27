#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate runtime probes for every supported interface storage case.

The binding maps the interface request to a concrete C++ request type and the
registration to the implementation storage plus interface list. The probes
exercise the real keyed/interface lookup path and the real conversion kernel.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

from tools.model.domain.cases import iter_cases, request_forms
from tools.model.emit.common.recipe import external_instance, registration
from tools.model.emit.common.runner import emit, render_template, trait_context
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar import cpp
from tools.model.grammar.expr import Expr
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY, LeafTraits
from tools.model.grammar.catalog import CONSUMING_SCOPES
from tools.model.grammar.storage import const_leaf, has_const
from tools.model.semantics.support import classify

COPYABLE_LEAF = "implementation_type"
MOVE_ONLY_LEAF = "move_only_implementation_type"


def _leaf(traits: LeafTraits) -> str:
    return COPYABLE_LEAF if traits.copyable else MOVE_ONLY_LEAF


def _is_const(case) -> bool:
    storage = case.storage_path.expr
    return const_leaf(storage) or (
        storage.op == "variant" and has_const(storage)
    )


def _registration(
    case, traits: LeafTraits = COPYABLE
) -> tuple[str, str]:
    path = case.storage_path
    form = path.form
    const = _is_const(case)
    leaf = _leaf(traits)
    storage = path.storage(leaf)
    interfaces = tuple(
        item.render()
        for item in case.binding.registration_interfaces(cpp.leaf(leaf))
    )
    factory = (
        path.const_factory(leaf) if const else path.factory(leaf)
    )
    if case.scope == "external":
        instance = external_instance(
            form,
            leaf,
            const=const,
            move=(
                not traits.copyable
                and path.expr.op != "lvalue_reference"
            ),
        )
        if instance is None:
            raise ValueError(f"{form.name} has no external instance")
        line, _ = registration(
            form,
            case.scope,
            "runtime",
            interfaces=interfaces,
            storage=storage,
            external_argument=instance.argument,
        )
        return instance.declaration, line
    line, _ = registration(
        form,
        case.scope,
        "runtime",
        interfaces=interfaces,
        storage=storage,
        factory=factory,
    )
    return "", line


def _binding(request: cpp.CppType) -> str:
    if request.kind is cpp.CppKind.REFERENCE:
        return "reference"
    if request.kind is cpp.CppKind.RVALUE_REFERENCE:
        return "rvalue"
    return "value"


def _slug(expr: Expr) -> str:
    return re.sub(r"[^a-z0-9]+", "_", expr.render().lower()).strip("_")


def build_rows(
    traits: LeafTraits = COPYABLE,
) -> list[dict[str, object]]:
    rows = []
    seen = set()
    for case in iter_cases(request_forms(), traits):
        if case.binding.kind != "interface":
            continue
        if case.scope in CONSUMING_SCOPES:
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
        instance, registration_line = _registration(case, traits)
        request_type = case.binding.request_type(case.request)
        digest = hashlib.sha1(case.request.render().encode()).hexdigest()[:8]
        rows.append(
            {
                "name": (
                    f"interface_{case.scope}_{case.storage_name}_"
                    f"{_slug(case.request)}_{digest}"
                ),
                "scope": case.scope,
                "storage": case.storage_path.expr.render(),
                "instance": instance,
                "registration": registration_line,
                "request": request_type.render(),
                "bind": _binding(request_type),
                "coverage": (*key, traits.name),
            }
        )
    return rows


def validate(traits: LeafTraits = COPYABLE) -> None:
    rows = build_rows(traits)
    if not rows:
        raise ValueError("interface runtime catalog must not be empty")
    names = [row["name"] for row in rows]
    if len(names) != len(set(names)):
        raise ValueError("interface runtime catalog contains duplicate names")
    for row in rows:
        if not row["registration"]:
            raise ValueError(f"{row['name']} has no registration")
        if not row["request"]:
            raise ValueError(f"{row['name']} has no request")


def render(output: Path, traits: LeafTraits = COPYABLE) -> str:
    validate(traits)
    context = trait_context(traits, "interface_runtime")
    context["leaf"] = _leaf(traits)
    return render_template(
        "interface_runtime.cpp.j2",
        rows=build_rows(traits),
        **context,
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
