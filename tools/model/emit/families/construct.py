#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Construct unregistered objects from a registered dependency.

The storage cell registers the dependency; the test calls
``container.construct<Constructed>()`` and checks the dependency was injected.
One file per storage cell and trait:

    resolution/runtime/<scope>/<storage>/construct/<traits>.cpp
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

from tools.model.emit.common.recipe import external_instance, registration
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.emit.hierarchy import CasePath
from tools.model.grammar import cpp
from tools.model.grammar.storage import STORAGE_PATHS, StoragePath, const_leaf, has_const
from tools.model.grammar.traits import TRAITS, LeafTraits
from tools.model.semantics.cells import iter_supported_cells

SKIP_KINDS = frozenset({"array", "array_handle"})


def _canonical_path(scope_name: str, form) -> StoragePath | None:
    expected = form.cpp_type("int")
    for path in STORAGE_PATHS:
        if (
            path.scope.name == scope_name
            and path.form_name == form.name
            and path.expr.with_leaf("int").cpp().render() == expected
        ):
            return path
    return None


def _fixture(traits: LeafTraits) -> str:
    return "dependency" if traits.copyable else "movable_dependency"


def _constructed(traits: LeafTraits, by_value: bool) -> str:
    if by_value:
        return "constructed_value" if traits.copyable else "movable_constructed_value"
    return "constructed" if traits.copyable else "movable_constructed"


def _registration(
    path: StoragePath, traits: LeafTraits
) -> tuple[str, str]:
    leaf = _fixture(traits)
    form = path.form
    storage = path.storage(leaf)
    factory = path.factory(leaf)
    if path.scope.name == "external":
        instance = external_instance(
            form,
            leaf,
            const=False,
            move=not traits.copyable,
        )
        if instance is None:
            return "", ""
        line, _ = registration(
            form,
            path.scope.name,
            "runtime",
            leaf=leaf,
            storage=storage,
            external_argument=instance.argument,
        )
        return instance.declaration, line
    line, _ = registration(
        form,
        path.scope.name,
        "runtime",
        leaf=leaf,
        storage=storage,
        factory=factory,
    )
    return "", line


@dataclass(frozen=True)
class ConstructShard:
    scope: str
    form: str
    traits: str
    path: str
    suite: str
    namespace: str
    instance: str
    registration: str
    dependency: str
    constructed: str
    expected: int


def build_shards() -> list[ConstructShard]:
    shards = []
    for traits in TRAITS:
        for scope, form in iter_supported_cells():
            if form.kind in SKIP_KINDS:
                continue
            if scope.name == "shared_cyclical":
                continue
            if scope.name == "unique" and form.kind not in {
                "value",
                "variant",
            }:
                continue

            path = _canonical_path(scope.name, form)
            if path is None:
                continue
            if const_leaf(path.expr) or (
                path.expr.op == "variant" and has_const(path.expr)
            ):
                continue
            instance, line = _registration(path, traits)
            if not line:
                continue
            case = CasePath(
                phase="resolution",
                container="runtime",
                scope=scope.name,
                storage=path.name,
                category="construct",
                traits=traits.name,
            )
            shards.append(
                ConstructShard(
                    scope=scope.name,
                    form=form.name,
                    traits=traits.name,
                    path=f"{case.file[:-4]}/{traits.name}.cpp",
                    suite=f"{case.suite}_{traits.name}",
                    namespace=f"{case.suite}_{traits.name}",
                    instance=instance,
                    registration=line,
                    dependency=_fixture(traits),
                    constructed=_constructed(
                        traits, scope.name in {"unique", "shared_cyclical"}
                    ),
                    expected=7 if traits.copyable else 9,
                )
            )
    return shards


def render(shard: ConstructShard) -> str:
    return render_template("construct_cells.cpp.j2", shard=shard)


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
    for shard in build_shards():
        emit(arguments.out_dir / shard.path, render(shard), check=arguments.check)


if __name__ == "__main__":
    main()
