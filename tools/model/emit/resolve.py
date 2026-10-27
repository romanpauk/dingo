#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate per-storage resolve probes for the supported path domain.

The output path mirrors the resolution hierarchy:

    runtime/<scope>/<storage>.cpp

One file is one storage path under one container front-end. A file contains a
single GTest suite named after that path, and one test per
``(traits, binding, request)``. A missing resolve is a skipped test and an
outcome line, not a build failure. The manifest is the model expectation; the
outcome report is what the implementation really resolves.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from tools.model.domain.requests import object_requests
from tools.model.emit.common.recipe import external_instance, registration
from tools.model.emit.hierarchy import CasePath
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar import cpp
from tools.model.grammar.bindings import (
    ANNOTATED,
    ANNOTATED_INTERFACE,
    CONCRETE,
    INTERFACE,
    KEYED,
)
from tools.model.grammar.catalog import CONSUMING_SCOPES
from tools.model.grammar.expr import Expr, annotated, interface, keyed
from tools.model.grammar.storage import (
    STORAGE_PATHS,
    StoragePath,
    const_leaf,
    has_const,
)
from tools.model.grammar.traits import TRAITS, LeafTraits
from tools.model.semantics.gaps import declared_runtime_gap
from tools.model.semantics.support import classify, to_shape

BINDINGS = (CONCRETE, ANNOTATED, KEYED, INTERFACE, ANNOTATED_INTERFACE)
CONTAINERS = ("runtime",)
# One file per category under the storage directory. The resolve probe is the
# first category; behavior, annotated, keyed, const, and interface tests can
# move under the same storage path later.
CATEGORIES = ("resolve",)


@dataclass(frozen=True)
class ResolveCase:
    name: str
    path: str
    category: str
    scope: str
    storage: str
    form: str
    binding: str
    traits: str
    request: str
    declared: bool
    declared_reason: str
    instance: str
    registration: str
    suite: str


@dataclass(frozen=True)
class Shard:
    container: str
    scope: str
    storage: str
    form: str
    category: str
    path: str
    suite: str
    cases: tuple[ResolveCase, ...]


def _is_const(path: StoragePath) -> bool:
    storage = path.expr
    return const_leaf(storage) or (
        storage.op == "variant" and has_const(storage)
    )


def _fixture(traits: LeafTraits) -> str:
    return "copyable" if traits.copyable else "movable"


def _project(request: Expr, binding) -> Expr:
    if binding.kind == "annotated":
        return annotated(request, binding.tag)
    if binding.kind == "keyed":
        return keyed(request, binding.key)
    if binding.kind == "interface":
        return interface(request, binding.interfaces[0])
    if binding.kind == "annotated_interface":
        return annotated(
            interface(request, binding.interfaces[0]),
            binding.tag,
        )
    return request


def _request_type(binding, request: Expr, fixture: str) -> str:
    if binding.kind in {"interface", "annotated_interface"}:
        return binding.request_type(request).render()
    return request.with_leaf(fixture).render()


def _slug(expr: Expr) -> str:
    op = expr.op
    if op == "leaf":
        return expr.name.lower()
    if op == "const":
        return "const_" + _slug(expr.inner)
    if op == "lvalue_reference":
        return _slug(expr.inner) + "_ref"
    if op == "rvalue_reference":
        return _slug(expr.inner) + "_rref"
    if op == "pointer":
        return _slug(expr.inner) + "_ptr"
    if op == "array":
        return _slug(expr.inner) + f"_array{expr.extent}"
    if op == "array_reference":
        return _slug(expr.inner) + f"_array_ref{expr.extent}"
    if op == "array_pointer":
        return _slug(expr.inner) + f"_array_ptr{expr.extent}"
    if op in {
        "optional",
        "unique_ptr",
        "shared_ptr",
        "unique_array",
        "shared_array",
    }:
        return f"{op}_{_slug(expr.inner)}"
    if op == "variant":
        return "variant_" + "_".join(
            _slug(item) for item in expr.alternatives
        )
    if op == "annotated":
        return f"annotated_{_slug(expr.inner)}_{expr.name}"
    if op == "keyed":
        return f"keyed_{expr.name}_{_slug(expr.inner)}"
    if op == "interface":
        return f"interface_{expr.name}_{_slug(expr.inner)}"
    raise ValueError(f"no slug for {op!r}")


def _skip_reason(path: StoragePath, binding) -> str | None:
    if (
        binding.kind in {"interface", "annotated_interface"}
        and path.scope.name in CONSUMING_SCOPES
    ):
        return "consuming scope cannot materialize an abstract interface value"
    if (
        path.scope.name == "shared_cyclical"
        and path.form.name == "pointer"
    ):
        return "cyclical pointer has no registration recipe yet"
    return None


def _registration(
    path: StoragePath, binding, traits: LeafTraits
) -> tuple[str, str]:
    const = _is_const(path)
    leaf = _fixture(traits)
    storage = path.storage(leaf)
    factory = (
        path.const_factory(leaf) if const else path.factory(leaf)
    )
    interfaces = tuple(
        item.render()
        for item in binding.registration_interfaces(cpp.leaf(leaf))
    )
    key = binding.key if binding.kind == "keyed" else None
    if path.scope.name == "external":
        if path.form.kind == "array":
            declaration = (
                cpp.declaration(path.cpp(leaf), "external_instance") + "{}"
            )
            line, _ = registration(
                path.form,
                path.scope.name,
                "runtime",
                leaf=leaf,
                interfaces=interfaces,
                key=key,
                storage=storage,
                external_argument="external_instance",
            )
            return declaration, line
        move = (
            not traits.copyable
            and path.expr.op != "lvalue_reference"
            and path.form.kind != "array"
        )
        instance = external_instance(
            path.form, leaf, const=const, move=move
        )
        if instance is None:
            return "", ""
        line, _ = registration(
            path.form,
            path.scope.name,
            "runtime",
            leaf=leaf,
            interfaces=interfaces,
            key=key,
            storage=storage,
            external_argument=instance.argument,
        )
        return instance.declaration, line
    line, _ = registration(
        path.form,
        path.scope.name,
        "runtime",
        leaf=leaf,
        interfaces=interfaces,
        key=key,
        storage=storage,
        factory=factory,
    )
    return "", line


def _cases_for(
    path: StoragePath, container: str, category: str
) -> tuple[ResolveCase, ...]:
    cases: list[ResolveCase] = []
    for traits in TRAITS:
        fixture = _fixture(traits)
        for binding in BINDINGS:
            if _skip_reason(path, binding) is not None:
                continue
            instance, line = _registration(path, binding, traits)
            if not line:
                continue
            for request in object_requests(path):
                projected = _project(request, binding)
                verdict = classify(
                    path.scope.name,
                    path.name,
                    binding,
                    projected,
                    traits,
                )
                if verdict.outcome != "supported":
                    continue
                shape = to_shape(projected)
                reason = declared_runtime_gap(
                    binding.kind,
                    path.scope.name,
                    path.form.kind,
                    shape.access if shape is not None else "",
                )
                slug = _slug(request)
                case_path = CasePath(
                    phase="resolution",
                    container=container,
                    scope=path.scope.name,
                    storage=path.name,
                    category=category,
                    traits=traits.name,
                    binding=binding.kind,
                    request=slug,
                )
                cases.append(
                    ResolveCase(
                        name=f"{traits.name}_{binding.kind}_{slug}",
                        path=case_path.path,
                        category=category,
                        scope=path.scope.name,
                        storage=path.name,
                        form=path.form.name,
                        binding=binding.kind,
                        traits=traits.name,
                        request=_request_type(binding, projected, fixture),
                        declared=reason is not None,
                        declared_reason=reason or "",
                        instance=instance,
                        registration=line,
                        suite=case_path.suite,
                    )
                )
    return tuple(cases)


def build_shards() -> tuple[Shard, ...]:
    grouped: dict[
        tuple[str, str, str, str], list[ResolveCase]
    ] = defaultdict(list)
    for container in CONTAINERS:
        for category in CATEGORIES:
            for path in STORAGE_PATHS:
                cases = _cases_for(path, container, category)
                if cases:
                    grouped[
                        (container, path.scope.name, path.name, category)
                    ].extend(cases)
    shards = []
    for (container, scope, storage, category), cases in sorted(
        grouped.items()
    ):
        suite = CasePath(
            phase="resolution",
            container=container,
            scope=scope,
            storage=storage,
            category=category,
        ).suite
        shards.append(
            Shard(
                container=container,
                scope=scope,
                storage=storage,
                form=cases[0].form,
                category=category,
                path=CasePath(
                    phase="resolution",
                    container=container,
                    scope=scope,
                    storage=storage,
                    category=category,
                ).file,
                suite=suite,
                cases=tuple(cases),
            )
        )
    return tuple(shards)


def render(shard: Shard) -> str:
    return render_template("resolve_cells.cpp.j2", shard=shard)


def manifest(shards: tuple[Shard, ...]) -> str:
    payload = {
        "shards": [
            {
                "container": shard.container,
                "scope": shard.scope,
                "storage": shard.storage,
                "form": shard.form,
                "category": shard.category,
                "path": shard.path,
                "suite": shard.suite,
                "cases": [
                    {
                        "name": case.name,
                        "path": case.path,
                        "scope": case.scope,
                        "storage": case.storage,
                        "form": case.form,
                        "category": case.category,
                        "binding": case.binding,
                        "traits": case.traits,
                        "request": case.request,
                        "declared": case.declared,
                        "declared_reason": case.declared_reason,
                        "instance": case.instance,
                        "registration": case.registration,
                    }
                    for case in shard.cases
                ],
            }
            for shard in shards
        ]
    }
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument(
        "--shard",
        action="append",
        help="only emit the named storage path (repeatable)",
    )
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    shards = build_shards()
    if arguments.shard:
        wanted = set(arguments.shard)
        shards = tuple(
            shard
            for shard in shards
            if shard.suite in wanted or shard.storage in wanted
        )
        if not shards:
            raise SystemExit(
                f"unknown shards: {', '.join(sorted(wanted))}"
            )
    for shard in shards:
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)
    if arguments.manifest is not None:
        emit(arguments.manifest, manifest(shards), check=arguments.check)


if __name__ == "__main__":
    main()
