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
import hashlib
import re
from pathlib import Path

from tools.model.domain.cases import iter_cases, request_forms
from tools.model.emit.common.recipe import external_instance, registration
from tools.model.emit.common.runner import emit, render_template
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar import cpp
from tools.model.grammar.expr import Expr
from tools.model.grammar.storage import const_leaf, has_const
from tools.model.semantics.support import to_shape
from tools.model.semantics.support import classify

KEY = "key_a"


def _is_const(case) -> bool:
    storage = case.storage_path.expr
    return const_leaf(storage) or (
        storage.op == "variant" and has_const(storage)
    )


def _registration(case) -> tuple[str, str]:
    path = case.storage_path
    form = path.form
    const = _is_const(case)
    storage = path.storage("tracked")
    factory = (
        path.const_factory("tracked") if const else path.factory("tracked")
    )
    if case.scope == "external":
        if form.kind == "array":
            declaration = cpp.declaration(path.cpp("tracked"), "external_instance") + "{}"
            line, _ = registration(
                form,
                case.scope,
                "runtime",
                key=KEY,
                storage=storage,
                external_argument="external_instance",
            )
            return declaration, line
        instance = external_instance(form, "tracked", const=const)
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

    The keyed runtime harness can address the registered cell through these
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


def build_rows() -> list[dict[str, object]]:
    rows = []
    seen = set()
    for case in iter_cases(request_forms()):
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
            case.scope, case.storage_name, case.binding, case.request
        )
        if verdict.outcome != "supported":
            continue
        key = (case.scope, case.storage_name, case.request)
        if key in seen:
            continue
        seen.add(key)
        instance, registration_line = _registration(case)
        digest = hashlib.sha1(case.request.render().encode()).hexdigest()[:8]
        rows.append(
            {
                "name": (
                    f"keyed_{case.scope}_{case.storage_name}_"
                    f"{_slug(case.request)}_{digest}"
                ),
                "scope": case.scope,
                "storage": case.storage_path.expr.render(),
                "instance": instance,
                "registration": registration_line,
                "request": case.request.with_leaf("tracked").render(),
                "bind": _binding(case.request),
                "coverage": key,
            }
        )
    return rows


def validate() -> None:
    rows = build_rows()
    if not rows:
        raise ValueError("keyed catalog must not be empty")
    names = [row["name"] for row in rows]
    if len(names) != len(set(names)):
        raise ValueError("keyed catalog contains duplicate names")
    for row in rows:
        if not row["registration"]:
            raise ValueError(f"{row['name']} has no registration")
        if not row["request"]:
            raise ValueError(f"{row['name']} has no request")


def render(output: Path) -> str:
    validate()
    return render_template("keyed_cells.cpp.j2", rows=build_rows())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--include",
        type=Path,
        help="include directory to validate the library catalogs against",
    )
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()

    if arguments.include is not None:
        validate_catalog(arguments.include)
    emit(arguments.out, render(arguments.out), check=arguments.check)


if __name__ == "__main__":
    main()
