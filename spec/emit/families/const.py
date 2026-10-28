#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate runtime probes for every supported const storage case.

The storage and request projections both come from `grammar/enumerate.py`. This family
does not carry a request list of its own: it walks the same case set the report
classifies, keeps the const-qualified storage cases that classify as supported,
and emits one probe per case. The registration is derived from the storage
form, so adding a const-capable form to the grammar adds probes here.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import re
from pathlib import Path

from spec.grammar.bindings import CONCRETE
from spec.domain.cases import iter_cases, request_forms
from spec.emit.common.recipe import external_instance, registration
from spec.emit.common.runner import emit, render_template
from spec.emit.hierarchy import CasePath
from spec.grammar.storage import const_leaf, has_const
from spec.emit.common.validation import validate_catalog
from spec.semantics.support import classify
from spec.grammar.expr import Expr
from spec.grammar.traits import COPYABLE, MOVE_ONLY, TRAITS, LeafTraits

STORAGE_SCOPES = frozenset({"shared", "external"})


def _registration(path, traits: LeafTraits = COPYABLE):
    form = path.form
    leaf = traits.cpp
    scope_type = path.scope.name
    if path.scope.name == "external":
        instance = external_instance(form, leaf, const=True)
        if instance is None:
            raise ValueError(f"{form.name} has no derived const instance")
        line, _ = registration(
            form,
            scope_type,
            "runtime",
            leaf=leaf,
            const=True,
            storage=path.storage(leaf),
            external_argument=instance.argument,
        )
        return instance.declaration, line
    line, _ = registration(
        form,
        scope_type,
        "runtime",
        leaf=leaf,
        const=True,
        storage=path.storage(leaf),
        factory=path.const_factory(leaf),
    )
    return "", line


def _binding(request: Expr) -> str:
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
        if case.binding.kind != CONCRETE.kind:
            continue
        if case.scope not in STORAGE_SCOPES:
            continue
        storage = case.storage_path.expr
        if not const_leaf(storage) and not (
            storage.op == "variant" and has_const(storage)
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
        instance, registration = _registration(case.storage_path, traits)
        name = (
            f"const_{case.scope}_{case.storage_name}_"
            f"{_slug(case.request)}_"
            f"{hashlib.sha1(case.request.render().encode()).hexdigest()[:8]}"
        )
        path = CasePath(
            phase="resolution",
            container="runtime",
            scope=case.scope,
            storage=case.storage_name,
            category="const",
            traits=traits.name,
            binding="concrete",
            request=f"{_slug(case.request)}_"
            f"{hashlib.sha1(case.request.render().encode()).hexdigest()[:8]}",
        )
        rows.append(
            {
                "name": name,
                "path": path.path,
                "suite": path.suite,
                "traits": traits.name,
                "storage_name": case.storage_name,
                "scope": case.scope,
                "storage": storage.render(),
                "instance": instance,
                "registration": registration,
                "request": case.request.with_leaf(traits.cpp).render(),
                "bind": _binding(case.request),
                "coverage": (*key, traits.name),
            }
        )
    return rows


def validate() -> None:
    rows = [row for traits in TRAITS for row in build_rows(traits)]
    if not rows:
        raise ValueError("const storage catalog must not be empty")
    paths = [row["path"] for row in rows]
    if len(paths) != len(set(paths)):
        raise ValueError("const storage catalog contains duplicate paths")
    for row in rows:
        if not row["registration"]:
            raise ValueError(f"{row['name']} has no registration")
        if not row["request"]:
            raise ValueError(f"{row['name']} has no request")


@dataclass(frozen=True)
class ConstShard:
    scope: str
    storage: str
    traits: str
    path: str
    suite: str
    namespace: str
    move_only: bool
    leaf: str
    rows: tuple[dict[str, object], ...]


def build_shards() -> list[ConstShard]:
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
            category="const",
            traits=traits_name,
        )
        traits = next(item for item in TRAITS if item.name == traits_name)
        shards.append(
            ConstShard(
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


def render(shard: ConstShard) -> str:
    return render_template(
        "const_cases.cpp.j2",
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
