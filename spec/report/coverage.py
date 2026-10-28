#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Compare the supported path set with the generated test suites.

The domain is generated in full. `semantics/support.py` removes what makes no sense
(unavailable/unsupported) and marks what is not modeled. The remaining
supported cases are compared with the cases the generated C++ suites actually
exercise. The difference is the coverage gap.

Every key carries the leaf trait: a copyable and a move-only leaf are separate
rows even when they share a storage path and request.
"""

from __future__ import annotations

from collections import Counter, defaultdict

from spec.model.semantics.capabilities import storage_form
from spec.model.domain.cases import iter_cases, request_forms
from spec.model.grammar.storage import const_leaf
from spec.model.grammar.traits import TRAITS
from spec.model.semantics.support import classify_case, to_shape
from spec.model.grammar.expr import family
from spec.model.semantics.gaps import declared_runtime_gap


def _key(case) -> tuple[str, str, object, str] | None:
    traits = case.traits.name
    if case.binding.kind in {
        "annotated",
        "annotated_interface",
        "keyed",
        "interface",
    }:
        return (case.scope, case.storage_name, case.request, traits)
    if case.binding.kind != "concrete":
        return None
    if const_leaf(case.storage_path.expr):
        return (case.scope, case.storage_name, case.request, traits)
    form = storage_form(case.storage_name)
    if form is None:
        return None
    shape = to_shape(case.request)
    if shape is None:
        return None
    return (case.scope, form.name, shape.name, traits)


def _compile_keys() -> set[tuple[str, str, object, str]]:
    from spec.gen.registration.binding import build_rows as binding_rows
    from spec.gen.resolution.const import build_rows as const_rows
    from spec.gen.resolution.type_table import SHAPES, build_rows as table_rows
    from spec.gen.resolution.trait_table import build_rows as trait_rows

    keys = set()
    # The type table is the copyable leaf's shape oracle. The trait table
    # adds the move-only answer and the per-shape binary probes.
    for row in table_rows():
        for shape in SHAPES:
            keys.add((row["scope"], row["form"], shape.name, "copyable"))
    for row in trait_rows():
        for shape in row["shapes"]:
            keys.add((row["scope"], row["form"], shape["name"], "copyable"))
            keys.add((row["scope"], row["form"], shape["name"], "move_only"))
    keys |= {row["coverage"] for row in binding_rows()}
    for traits in TRAITS:
        keys |= {row["coverage"] for row in const_rows(traits)}
    return keys


def _runtime_keys() -> set[tuple[str, str, object, str]]:
    from spec.gen.resolution.type_behavior import build_rows as shape_rows
    from spec.gen.resolution.annotated import build_rows as annotated_rows
    from spec.gen.resolution.const import build_rows as const_rows
    from spec.gen.resolution.interface_runtime import (
        build_rows as interface_rows,
    )
    from spec.gen.resolution.keyed import build_rows as keyed_rows

    keys = set()
    for traits in TRAITS:
        for row in shape_rows(("runtime",), traits):
            for shape in row["shapes"]:
                if not shape["gap"]:
                    keys.add(
                        (row["scope"], row["form"], shape["name"], traits.name)
                    )
        keys |= {
            row["coverage"] for row in annotated_rows(traits)
        }
        keys |= {row["coverage"] for row in const_rows(traits)}
        keys |= {row["coverage"] for row in keyed_rows(traits)}
        keys |= {row["coverage"] for row in interface_rows(traits)}
    return keys


def coverage_report(examples: int = 4) -> dict[str, object]:
    compile_keys = _compile_keys()
    runtime_keys = _runtime_keys()
    supported = 0
    compile_tested = 0
    runtime_tested = 0
    gap_by_binding: Counter[str] = Counter()
    runtime_gap_by_binding: Counter[str] = Counter()
    declared_by_binding: Counter[str] = Counter()
    declared_examples: dict[str, list[str]] = defaultdict(list)
    gap_by_family: Counter[str] = Counter()
    examples_by_binding: dict[str, list[str]] = defaultdict(list)
    supported_by_trait: Counter[str] = Counter()
    compile_by_trait: Counter[str] = Counter()
    runtime_by_trait: Counter[str] = Counter()
    gap_by_trait: Counter[str] = Counter()
    declared_by_trait: Counter[str] = Counter()

    for traits in TRAITS:
        for case in iter_cases(request_forms(), traits):
            verdict = classify_case(case)
            if verdict.outcome != "supported":
                continue
            supported += 1
            supported_by_trait[traits.name] += 1
            key = _key(case)
            in_compile = key is not None and key in compile_keys
            in_runtime = key is not None and key in runtime_keys
            compile_tested += int(in_compile)
            runtime_tested += int(in_runtime)
            compile_by_trait[traits.name] += int(in_compile)
            runtime_by_trait[traits.name] += int(in_runtime)
            if not in_compile:
                gap_by_binding[case.binding.kind] += 1
                gap_by_family[family(case.request)] += 1
                if len(examples_by_binding[case.binding.kind]) < examples:
                    examples_by_binding[case.binding.kind].append(
                        f"{case.scope}/{case.storage_name}: "
                        f"{case.request.render()} [{traits.name}]"
                    )
            if not in_runtime:
                shape = to_shape(case.request)
                reason = declared_runtime_gap(
                    case.binding.kind,
                    case.scope,
                    case.storage_path.form.kind,
                    shape.access if shape is not None else "",
                )
                if reason is not None:
                    declared_by_binding[case.binding.kind] += 1
                    declared_by_trait[traits.name] += 1
                    if len(declared_examples[reason]) < examples:
                        declared_examples[reason].append(
                            f"{case.scope}/{case.storage_name}: "
                            f"{case.request.render()} [{traits.name}]"
                        )
                else:
                    runtime_gap_by_binding[case.binding.kind] += 1
                    gap_by_trait[traits.name] += 1

    return {
        "supported": supported,
        "compile_tested": compile_tested,
        "runtime_tested": runtime_tested,
        "compile_gap": supported - compile_tested,
        "runtime_gap": sum(runtime_gap_by_binding.values()),
        "declared_gap": sum(declared_by_binding.values()),
        "supported_by_trait": dict(supported_by_trait.most_common()),
        "compile_by_trait": dict(compile_by_trait.most_common()),
        "runtime_by_trait": dict(runtime_by_trait.most_common()),
        "compile_gap_by_trait": {
            traits.name: supported_by_trait[traits.name]
            - compile_by_trait[traits.name]
            for traits in TRAITS
        },
        "runtime_gap_by_trait": dict(gap_by_trait.most_common()),
        "declared_gap_by_trait": dict(declared_by_trait.most_common()),
        "declared_gap_by_binding": dict(declared_by_binding.most_common()),
        "declared_gap_examples": dict(declared_examples),
        "gap_by_binding": {
            name: gap_by_binding.get(name, 0)
            for name in sorted(
                {
                    "annotated",
                    "annotated_interface",
                    "concrete",
                    "interface",
                    "keyed",
                }
                | set(gap_by_binding)
                | set(runtime_gap_by_binding)
            )
        },
        "runtime_gap_by_binding": {
            name: runtime_gap_by_binding.get(name, 0)
            for name in sorted(
                {
                    "annotated",
                    "annotated_interface",
                    "concrete",
                    "interface",
                    "keyed",
                }
                | set(gap_by_binding)
                | set(runtime_gap_by_binding)
            )
        },
        "gap_by_family": dict(gap_by_family.most_common()),
        "examples": dict(examples_by_binding),
    }
