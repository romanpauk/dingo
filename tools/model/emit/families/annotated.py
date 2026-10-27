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
from dataclasses import dataclass
from pathlib import Path

from tools.model.grammar import cpp
from tools.model.grammar.catalog import FORM_BY_NAME
from tools.model.domain.cases import iter_cases, request_forms
from tools.model.emit.common.recipe import external_instance, registration
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.hierarchy import CasePath
from tools.model.emit.common.validation import validate_catalog
from tools.model.semantics.support import classify
from tools.model.grammar.expr import Expr
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY, TRAITS, LeafTraits
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
        slug = f"annotated_{case.storage_name}_{len(rows)}"
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope=case.scope,
            storage=case.storage_name,
            category="annotated",
            traits=traits.name,
            binding="annotated",
            request=slug,
        )
        rows.append(
            {
                "name": slug,
                "path": path.path,
                "scope": case.scope,
                "form": case.storage_name,
                "traits": traits.name,
                "instance": instance,
                "registration": registration_line,
                "request": case.request.with_leaf(traits.cpp).render(),
                "relation": relation,
                "bind_reference": bind_reference,
                "coverage": (*key, traits.name),
            }
        )
    return rows


def validate() -> None:
    rows = [row for traits in TRAITS for row in build_rows(traits)]
    if not rows:
        raise ValueError("annotated catalog must not be empty")
    paths = [row["path"] for row in rows]
    if len(paths) != len(set(paths)):
        raise ValueError("annotated catalog contains duplicate paths")
    for row in rows:
        if not row["registration"]:
            raise ValueError(f"{row['name']} has no registration")
        if not row["request"]:
            raise ValueError(f"{row['name']} has no request")


@dataclass(frozen=True)
class AnnotatedShard:
    scope: str
    storage: str
    traits: str
    path: str
    suite: str
    namespace: str
    move_only: bool
    leaf: str
    rows: tuple[dict[str, object], ...]


def build_shards() -> list[AnnotatedShard]:
    grouped: dict[tuple[str, str, str], list[dict[str, object]]] = {}
    for traits in TRAITS:
        for row in build_rows(traits):
            key = (str(row["scope"]), str(row["form"]), traits.name)
            grouped.setdefault(key, []).append(row)
    shards = []
    for (scope, storage, traits_name), rows in sorted(grouped.items()):
        prefix = CasePath(
            phase="resolution",
            container="runtime",
            scope=scope,
            storage=storage,
            category="annotated",
            traits=traits_name,
        )
        traits = next(item for item in TRAITS if item.name == traits_name)
        shards.append(
            AnnotatedShard(
                scope=scope,
                storage=storage,
                traits=traits_name,
                path=f"{prefix.file[:-4]}/{traits_name}.cpp",
                suite=f"{prefix.suite}_{traits_name}",
                namespace=f"{prefix.suite}_{traits_name}",
                move_only=not traits.copyable,
                leaf=traits.cpp,
                rows=tuple(rows),
            )
        )
    return shards


def render(shard: AnnotatedShard) -> str:
    return render_template(
        "annotated_cases.cpp.j2",
        rows=shard.rows,
        namespace=shard.namespace,
        suite=shard.suite,
        leaf=shard.leaf,
        move_only=shard.move_only,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    validate()
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
