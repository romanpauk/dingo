#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Generate compile-time binding registration probes.

The output follows the shared hierarchy:

    registration/compile/<scope>/<storage>/binding.cpp

One file is one storage path, one suite is that path, and one test is one
``(traits, binding)`` pair. The test instantiates
``dingo::bindings<dingo::bind<...>>`` for the binding composition; the requests
the binding supports are recorded in the manifest for coverage.
"""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from tools.model.domain.cases import iter_cases, request_forms
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.emit.hierarchy import CasePath
from tools.model.grammar import cpp
from tools.model.grammar.expr import Expr
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY, LeafTraits
from tools.model.semantics.support import classify

COPYABLE_LEAF = "implementation_type"
MOVE_ONLY_LEAF = "move_only_implementation_type"
BINDINGS = frozenset(
    {"annotated", "annotated_interface", "keyed", "interface"}
)


def _leaf(traits: LeafTraits) -> str:
    return COPYABLE_LEAF if traits.copyable else MOVE_ONLY_LEAF


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
        path = CasePath(
            phase="registration",
            container="compile",
            scope=case.scope,
            storage=case.storage_name,
            category="binding",
            traits=traits.name,
            binding=case.binding.kind,
        )
        rows.append(
            {
                "name": f"{traits.name}_{case.binding.kind}",
                "scope": case.scope,
                "storage": case.storage_name,
                "binding": case.binding.kind,
                "traits": traits.name,
                "request": case.request.render(),
                "binding_type": _binding_type(case, traits),
                "path": path.path,
                "file": path.file,
                "suite": path.suite,
                "coverage": (*key, traits.name),
            }
        )
    return rows


def build_rows() -> list[dict[str, object]]:
    return [*_rows_for(COPYABLE), *_rows_for(MOVE_ONLY)]


@dataclass(frozen=True)
class BindingCase:
    name: str
    traits: str
    binding: str
    binding_type: str
    path: str
    requests: tuple[str, ...]


@dataclass(frozen=True)
class BindingShard:
    scope: str
    storage: str
    path: str
    suite: str
    cases: tuple[BindingCase, ...]


def build_shards() -> tuple[BindingShard, ...]:
    grouped: dict[tuple[str, str], list[dict[str, object]]] = defaultdict(list)
    for row in build_rows():
        grouped[(row["scope"], row["storage"])].append(row)
    shards = []
    for (scope, storage), rows in sorted(grouped.items()):
        by_case: dict[tuple[str, str], list[dict[str, object]]] = defaultdict(list)
        for row in rows:
            by_case[(row["traits"], row["binding"])].append(row)
        cases = []
        for (traits, binding), case_rows in sorted(by_case.items()):
            first = case_rows[0]
            cases.append(
                BindingCase(
                    name=f"{traits}_{binding}",
                    traits=traits,
                    binding=binding,
                    binding_type=first["binding_type"],
                    path=CasePath(
                        phase="registration",
                        container="compile",
                        scope=scope,
                        storage=storage,
                        category="binding",
                        traits=traits,
                        binding=binding,
                    ).path,
                    requests=tuple(str(row["request"]) for row in case_rows),
                )
            )
        prefix = CasePath(
            phase="registration",
            container="compile",
            scope=scope,
            storage=storage,
            category="binding",
        )
        shards.append(
            BindingShard(
                scope=scope,
                storage=storage,
                path=prefix.file,
                suite=prefix.suite,
                cases=tuple(cases),
            )
        )
    return tuple(shards)


def render(shard: BindingShard) -> str:
    return render_template("binding_matrix.cpp.j2", shard=shard)


def manifest(shards: tuple[BindingShard, ...]) -> str:
    payload = {
        "shards": [
            {
                "scope": shard.scope,
                "storage": shard.storage,
                "path": shard.path,
                "suite": shard.suite,
                "cases": [
                    {
                        "name": case.name,
                        "path": case.path,
                        "traits": case.traits,
                        "binding": case.binding,
                        "requests": list(case.requests),
                    }
                    for case in shard.cases
                ],
            }
            for shard in shards
        ]
    }
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def validate() -> None:
    shards = build_shards()
    if not shards:
        raise ValueError("binding matrix must not be empty")
    paths = [shard.path for shard in shards]
    if len(paths) != len(set(paths)):
        raise ValueError("binding matrix contains duplicate shards")
    for shard in shards:
        if not shard.cases:
            raise ValueError(f"shard {shard.path} has no cases")
        for case in shard.cases:
            if not case.binding_type:
                raise ValueError(f"{case.path} has no binding type")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
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
    if arguments.manifest is not None:
        emit(
            arguments.manifest,
            manifest(build_shards()),
            check=arguments.check,
        )


if __name__ == "__main__":
    main()
