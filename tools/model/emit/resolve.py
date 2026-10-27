#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate per-storage resolve probes for the supported path domain.

This is the runtime counterpart of the shape matrix. Instead of asking the
compile-time route table whether a shape is published, it registers each
storage once per binding and calls ``container.resolve<Request>()`` for every
supported case. Each shard covers one ``(scope, form)`` cell and contains the
concrete storage paths of that cell, so a compile failure maps back to a
storage.

Outcomes are written as JSONL rather than asserted. The generated report is the
oracle for what the implementation really resolves; the model expectation is in
the manifest. This lets the model be tightened against the implementation
instead of the other way around.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from tools.model.domain.requests import object_requests
from tools.model.emit.common.recipe import external_instance, registration
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
from tools.model.grammar.expr import Expr, annotated, interface, keyed
from tools.model.grammar.catalog import CONSUMING_SCOPES, FormConstructor
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


@dataclass(frozen=True)
class ResolveCall:
    name: str
    request: str
    expression: str
    declared: bool
    declared_reason: str = ""


@dataclass(frozen=True)
class ResolveTest:
    name: str
    scope: str
    storage: str
    binding: str
    traits: str
    form: str
    instance: str
    registration: str
    calls: tuple[ResolveCall, ...]


@dataclass(frozen=True)
class Shard:
    name: str
    scope: str
    form: str
    tests: tuple[ResolveTest, ...]


def _is_const(path: StoragePath) -> bool:
    storage = path.expr
    return const_leaf(storage) or (
        storage.op == "variant" and has_const(storage)
    )


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


def _request_type(binding, request: Expr, leaf: str) -> str:
    if binding.kind in {"interface", "annotated_interface"}:
        return binding.request_type(request).render()
    return request.with_leaf(leaf).render()


def _resolve_expression(binding, request: Expr, leaf: str) -> str:
    request_type = _request_type(binding, request, leaf)
    if binding.kind == "keyed":
        return (
            f"container.template resolve<{request_type}>("
            f"dingo::key_type<{binding.key}>{{}})"
        )
    return f"container.template resolve<{request_type}>()"


def _slug(expr: Expr) -> str:
    return re.sub(r"[^a-z0-9]+", "_", expr.render().lower()).strip("_")


def _test_rows(
    path: StoragePath,
    binding,
    traits: LeafTraits,
) -> tuple[ResolveCall, ...]:
    calls = []
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
        declared = reason is not None
        request_type = _request_type(binding, projected, traits.cpp)
        digest = hashlib.sha1(
            projected.render().encode()
        ).hexdigest()[:8]
        calls.append(
            ResolveCall(
                name=(
                    f"{path.scope.name}_{path.name}_{binding.kind}_"
                    f"{_slug(projected)}_{traits.name}_{digest}"
                ),
                request=request_type,
                expression=_resolve_expression(binding, projected, traits.cpp),
                declared=declared,
                declared_reason=reason or "",
            )
        )
    return tuple(calls)


def _registration(
    path: StoragePath,
    binding,
    traits: LeafTraits,
) -> tuple[str, str]:
    # A consuming scope cannot materialize an abstract interface value; the
    # declared interface gap is a registration limitation, not a route probe.
    if (
        binding.kind in {"interface", "annotated_interface"}
        and path.scope.name in CONSUMING_SCOPES
    ):
        return "", ""
    # The cyclical pointer cell has no valid factory recipe yet; it is a
    # declared graph-scenario gap.
    if (
        path.scope.name == "shared_cyclical"
        and path.form.name == "pointer"
    ):
        return "", ""
    const = _is_const(path)
    leaf = traits.cpp
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
            path.form,
            leaf,
            const=const,
            move=move,
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


def build_shards(
    traits: tuple[LeafTraits, ...] = TRAITS,
) -> tuple[Shard, ...]:
    grouped: dict[tuple[str, str], list[ResolveTest]] = defaultdict(list)
    for trait in traits:
        for path in STORAGE_PATHS:
            for binding in BINDINGS:
                calls = _test_rows(path, binding, trait)
                if not calls:
                    continue
                instance, line = _registration(path, binding, trait)
                if not line:
                    continue
                grouped[(path.scope.name, path.form.name)].append(
                    ResolveTest(
                        name=(
                            f"{trait.name}_{path.scope.name}_{path.name}_"
                            f"{binding.kind}"
                        ),
                        scope=path.scope.name,
                        storage=path.name,
                        binding=binding.kind,
                        traits=trait.name,
                        form=path.form.name,
                        instance=instance,
                        registration=line,
                        calls=calls,
                    )
                )
    return tuple(
        Shard(
            name=f"{scope}_{form}",
            scope=scope,
            form=form,
            tests=tuple(tests),
        )
        for (scope, form), tests in sorted(grouped.items())
    )


def render(shard: Shard) -> str:
    return render_template(
        "resolve_cells.cpp.j2",
        shard=shard,
        traits=TRAITS,
    )


def manifest(shards: tuple[Shard, ...]) -> str:
    payload = {
        "shards": [
            {
                "name": shard.name,
                "scope": shard.scope,
                "form": shard.form,
                "tests": [
                    {
                        "name": test.name,
                        "scope": test.scope,
                        "storage": test.storage,
                        "binding": test.binding,
                        "traits": test.traits,
                        "form": test.form,
                        "calls": [
                            {
                                "name": call.name,
                                "request": call.request,
                                "declared": call.declared,
                                "declared_reason": call.declared_reason,
                            }
                            for call in test.calls
                        ],
                    }
                    for test in shard.tests
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
        help="only emit the named shard (repeatable)",
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
        shards = tuple(shard for shard in shards if shard.name in wanted)
        missing = wanted - {shard.name for shard in shards}
        if missing:
            raise SystemExit(f"unknown shards: {', '.join(sorted(missing))}")
    arguments.out_dir.mkdir(parents=True, exist_ok=True)
    for shard in shards:
        emit(
            arguments.out_dir / f"{shard.name}.cpp",
            render(shard),
            check=arguments.check,
        )
    if arguments.manifest is not None:
        emit(arguments.manifest, manifest(shards), check=arguments.check)


if __name__ == "__main__":
    main()
