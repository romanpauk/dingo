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
"""

from __future__ import annotations

from collections import Counter, defaultdict

from tools.model.semantics.capabilities import storage_form
from tools.model.domain.cases import iter_cases, request_forms
from tools.model.grammar.storage import const_leaf
from tools.model.semantics.support import classify_case, to_shape
from tools.model.grammar.expr import family
from tools.model.semantics.gaps import declared_runtime_gap


def _key(case) -> tuple[str, str, object] | None:
    if case.binding.kind in {"annotated", "keyed", "interface"}:
        return (case.scope, case.storage_name, case.request)
    if case.binding.kind != "concrete":
        return None
    if const_leaf(case.storage_path.expr):
        return (case.scope, case.storage_name, case.request)
    form = storage_form(case.storage_name)
    if form is None:
        return None
    shape = to_shape(case.request)
    if shape is None:
        return None
    return (case.scope, form.name, shape.name)


def _compile_keys() -> set[tuple[str, str, str]]:
    from tools.model.emit.binding_matrix import build_rows as binding_rows
    from tools.model.emit.families.const import build_rows as const_rows
    from tools.model.emit.matrix import SHAPES, build_rows

    keys = {
        (row["scope"], row["form"], shape.name)
        for row in build_rows()
        for shape in SHAPES
    }
    keys |= {row["coverage"] for row in binding_rows()}
    # Const storage paths are not in the concrete shape matrix; the const
    # family compiles one probe per supported case.
    keys |= {row["coverage"] for row in const_rows()}
    return keys


def _runtime_keys() -> set[tuple[str, str, str]]:
    from tools.model.emit.families.annotated import build_rows as annotated_rows
    from tools.model.emit.families.const import build_rows as const_rows
    from tools.model.emit.families.interface_runtime import (
        build_rows as interface_rows,
    )
    from tools.model.emit.families.keyed import build_rows as keyed_rows
    from tools.model.emit.behavior import build_rows

    keys = set()
    for row in build_rows():
        for shape in row["shapes"]:
            if not shape["gap"]:
                keys.add((row["scope"], row["form"], shape["name"]))
    keys |= {row["coverage"] for row in annotated_rows()}
    keys |= {row["coverage"] for row in const_rows()}
    keys |= {row["coverage"] for row in keyed_rows()}
    keys |= {row["coverage"] for row in interface_rows()}
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

    for case in iter_cases(request_forms()):
        verdict = classify_case(case)
        if verdict.outcome != "supported":
            continue
        supported += 1
        key = _key(case)
        in_compile = key is not None and key in compile_keys
        in_runtime = key is not None and key in runtime_keys
        compile_tested += int(in_compile)
        runtime_tested += int(in_runtime)
        if not in_compile:
            gap_by_binding[case.binding.kind] += 1
            gap_by_family[family(case.request)] += 1
            if len(examples_by_binding[case.binding.kind]) < examples:
                examples_by_binding[case.binding.kind].append(
                    f"{case.scope}/{case.storage_name}: {case.request.render()}"
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
                if len(declared_examples[reason]) < examples:
                    declared_examples[reason].append(
                        f"{case.scope}/{case.storage_name}: "
                        f"{case.request.render()}"
                    )
            else:
                runtime_gap_by_binding[case.binding.kind] += 1

    return {
        "supported": supported,
        "compile_tested": compile_tested,
        "runtime_tested": runtime_tested,
        "compile_gap": supported - compile_tested,
        "runtime_gap": sum(runtime_gap_by_binding.values()),
        "declared_gap": sum(declared_by_binding.values()),
        "declared_gap_by_binding": dict(declared_by_binding.most_common()),
        "declared_gap_examples": dict(declared_examples),
        "gap_by_binding": {
            name: gap_by_binding.get(name, 0)
            for name in sorted(
                {"annotated", "concrete", "interface", "keyed"}
                | set(gap_by_binding)
                | set(runtime_gap_by_binding)
            )
        },
        "runtime_gap_by_binding": {
            name: runtime_gap_by_binding.get(name, 0)
            for name in sorted(
                {"annotated", "concrete", "interface", "keyed"}
                | set(gap_by_binding)
                | set(runtime_gap_by_binding)
            )
        },
        "gap_by_family": dict(gap_by_family.most_common()),
        "examples": dict(examples_by_binding),
    }
