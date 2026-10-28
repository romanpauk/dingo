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
from spec.grammar.traits import COPYABLE, MOVE_ONLY, TRAITS, LeafTraits
from spec.grammar.catalog import CONSUMING_SCOPES
from spec.grammar.storage import const_leaf, has_const
from spec.semantics.support import classify

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
    if request.kind is cpp.CppKind.TEMPLATE and request.name == "dingo::annotated":
        # The annotation wrapper is transparent to the reference category.
        return _binding(request.args[0])
    return "value"


def _slug(expr: Expr) -> str:
    return re.sub(r"[^a-z0-9]+", "_", expr.render().lower()).strip("_")


def build_rows(
    traits: LeafTraits = COPYABLE,
) -> list[dict[str, object]]:
    rows = []
    seen = set()
    for case in iter_cases(request_forms(), traits):
        if case.binding.kind not in {"interface", "annotated_interface"}:
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
        name = (
            f"interface_{case.scope}_{case.storage_name}_"
            f"{_slug(case.request)}_{digest}"
        )
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope=case.scope,
            storage=case.storage_name,
            category="interface",
            traits=traits.name,
            binding=case.binding.kind,
            request=f"{_slug(case.request)}_{digest}",
        )
        rows.append(
            {
                "name": name,
                "path": path.path,
                "suite": path.suite,
                "traits": traits.name,
                "storage_name": case.storage_name,
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


def validate() -> None:
    rows = [row for traits in TRAITS for row in build_rows(traits)]
    if not rows:
        raise ValueError("interface runtime catalog must not be empty")
    paths = [row["path"] for row in rows]
    if len(paths) != len(set(paths)):
        raise ValueError("interface runtime catalog contains duplicate paths")
    for row in rows:
        if not row["registration"]:
            raise ValueError(f"{row['name']} has no registration")
        if not row["request"]:
            raise ValueError(f"{row['name']} has no request")


@dataclass(frozen=True)
class InterfaceShard:
    scope: str
    storage: str
    traits: str
    path: str
    suite: str
    namespace: str
    move_only: bool
    leaf: str
    rows: tuple[dict[str, object], ...]


def build_shards() -> list[InterfaceShard]:
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
            category="interface",
            traits=traits_name,
        )
        traits = next(item for item in TRAITS if item.name == traits_name)
        shards.append(
            InterfaceShard(
                scope=scope,
                storage=storage,
                traits=traits_name,
                path=f"{prefix.file[:-4]}/{traits_name}.cpp",
                suite=f"{prefix.suite}_{traits_name}",
                namespace=f"{prefix.suite}_{traits_name}",
                move_only=not traits.copyable,
                leaf=_leaf(traits),
                rows=tuple(rows),
            )
        )
    return shards


def render(shard: InterfaceShard) -> str:
    return render_template(
        "interface_runtime.cpp.j2",
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
