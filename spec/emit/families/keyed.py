#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate runtime probes for every supported keyed storage case.

The keyed binding is registration-level: the storage path and the request come
from the same grammar as the other families, and the key is applied at
registration and resolution. The probes prove the keyed route is registered and
addressable; they do not pin identity or copy/move counts.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import re
from pathlib import Path

from spec.domain.cases import iter_cases, request_forms
from spec.emit.common.recipe import external_instance, registration
from spec.emit.common.runner import emit, render_template
from spec.emit.hierarchy import CasePath
from spec.emit.common.validation import validate_catalog
from spec.grammar import cpp
from spec.grammar.expr import Expr
from spec.grammar.storage import const_leaf, has_const
from spec.grammar.traits import COPYABLE, MOVE_ONLY, TRAITS, LeafTraits
from spec.semantics.support import to_shape
from spec.semantics.support import classify

KEY = "key_a"


def _is_const(case) -> bool:
    storage = case.storage_path.expr
    return const_leaf(storage) or (
        storage.op == "variant" and has_const(storage)
    )


def _registration(case, traits: LeafTraits = COPYABLE) -> tuple[str, str]:
    path = case.storage_path
    form = path.form
    const = _is_const(case)
    storage = path.storage(traits.cpp)
    factory = (
        path.const_factory(traits.cpp)
        if const
        else path.factory(traits.cpp)
    )
    if case.scope == "external":
        if form.kind == "array":
            declaration = (
                cpp.declaration(path.cpp(traits.cpp), "external_instance")
                + "{}"
            )
            line, _ = registration(
                form,
                case.scope,
                "runtime",
                key=KEY,
                storage=storage,
                external_argument="external_instance",
            )
            return declaration, line
        instance = external_instance(
            form,
            traits.cpp,
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
            key=KEY,
            storage=storage,
            external_argument=instance.argument,
        )
        return instance.declaration, line
    line, _ = registration(
        form,
        case.scope,
        "runtime",
        key=KEY,
        storage=storage,
        factory=factory,
    )
    return "", line


def _unqualified(request: Expr) -> Expr:
    while request.op in {"keyed", "annotated", "interface"}:
        request = request.inner
    return request


def _direct_leaf_request(request: Expr) -> bool:
    """Whether the request is a direct value/reference/pointer to the leaf.

    The keyed runtime harness can address the registered form through these
    forms. Wrapped requests (optional/handle/variant conversions) are part of
    the classified keyed set but remain in the coverage gap until their routes
    are verified.
    """
    while request.op in {
        "keyed",
        "annotated",
        "interface",
        "lvalue_reference",
        "rvalue_reference",
        "pointer",
        "const",
    }:
        request = request.inner
    return request.op == "leaf"


def _binding(request: Expr) -> str:
    while request.op in {"keyed", "annotated", "interface"}:
        request = request.inner
    if request.op == "lvalue_reference":
        return "reference"
    if request.op == "rvalue_reference":
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
        if case.binding.kind != "keyed":
            continue
        if not _direct_leaf_request(case.request):
            continue
        form = case.storage_path.form
        shape = to_shape(_unqualified(case.request))
        if form.kind == "array":
            if (
                case.storage_path.expr.op == "array"
                and case.storage_path.expr.inner.op == "array"
            ):
                continue
            if case.scope == "unique" and (
                shape is None or shape.access != "pointer"
            ):
                continue
        if case.scope == "unique" and form.kind in {
            "handle",
            "array_handle",
            "optional",
        }:
            continue
        if (
            case.scope == "unique"
            and form.kind == "pointer"
            and (shape is None or shape.access != "pointer")
        ):
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
        digest = hashlib.sha1(case.request.render().encode()).hexdigest()[:8]
        name = (
            f"keyed_{case.scope}_{case.storage_name}_"
            f"{_slug(case.request)}_{digest}"
        )
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope=case.scope,
            storage=case.storage_name,
            category="keyed",
            traits=traits.name,
            binding="keyed",
            request=f"{_slug(case.request)}_{digest}",
        )
        rows.append(
            {
                "name": name,
                "path": path.path,
                "suite": path.suite,
                "traits": traits.name,
                "scope": case.scope,
                "storage": case.storage_path.expr.render(),
                "storage_name": case.storage_name,
                "instance": instance,
                "registration": registration_line,
                "request": case.request.with_leaf(traits.cpp).render(),
                "bind": _binding(case.request),
                "coverage": (*key, traits.name),
            }
        )
    return rows


def validate() -> None:
    rows = [row for traits in TRAITS for row in build_rows(traits)]
    if not rows:
        raise ValueError("keyed catalog must not be empty")
    paths = [row["path"] for row in rows]
    if len(paths) != len(set(paths)):
        raise ValueError("keyed catalog contains duplicate paths")
    for row in rows:
        if not row["registration"]:
            raise ValueError(f"{row['name']} has no registration")
        if not row["request"]:
            raise ValueError(f"{row['name']} has no request")


@dataclass(frozen=True)
class KeyedShard:
    scope: str
    storage: str
    traits: str
    path: str
    suite: str
    namespace: str
    move_only: bool
    leaf: str
    rows: tuple[dict[str, object], ...]


def build_shards() -> list[KeyedShard]:
    grouped: dict[tuple[str, str, str], list[dict[str, object]]] = {}
    for traits in TRAITS:
        for row in build_rows(traits):
            key = (str(row["scope"]), str(row["storage_name"]), traits.name)
            grouped.setdefault(key, []).append(row)
    shards = []
    for (scope, storage, traits_name), rows in sorted(grouped.items()):
        prefix = CasePath(
            phase="resolution",
            container="runtime",
            scope=scope,
            storage=storage,
            category="keyed",
            traits=traits_name,
        )
        traits = next(item for item in TRAITS if item.name == traits_name)
        shards.append(
            KeyedShard(
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


def render(shard: KeyedShard) -> str:
    return render_template(
        "keyed_cases.cpp.j2",
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
