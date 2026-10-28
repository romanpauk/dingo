#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Derive request types from each stored type.

The stored expression and its leaf are the seed. A bounded constructor closure
produces the request spellings, then the binding projections are applied.
Support classification is deliberately separate.
"""

from __future__ import annotations

from functools import lru_cache

from spec.model.grammar.bindings import (
    ANNOTATED,
    ANNOTATED_INTERFACE,
    CONCRETE,
    INTERFACE,
    KEYED,
    Binding,
)
from spec.model.grammar.expr import (
    Expr,
    annotated,
    array,
    array_pointer,
    array_reference,
    const,
    interface,
    keyed,
    leaf,
    lvalue_reference,
    optional,
    pointer,
    rvalue_reference,
    shared_array,
    shared_ptr,
    unique_array,
    unique_ptr,
    valid,
    variant,
)
from spec.model.grammar.storage import STORAGE_PATHS, StoragePath

DEPTH = 2
CONSTRUCTORS = (
    const,
    lvalue_reference,
    rvalue_reference,
    pointer,
    optional,
    unique_ptr,
    shared_ptr,
    unique_array,
    shared_array,
)
ARRAY_CONSTRUCTORS = (array, array_reference, array_pointer)


def _add(target: set[Expr], expr: Expr | None) -> None:
    if expr is not None and valid(expr):
        target.add(expr)


@lru_cache(maxsize=None)
def object_requests(storage: StoragePath, depth: int = DEPTH) -> tuple[Expr, ...]:
    base = {leaf("T"), storage.expr}
    forms: set[Expr] = set(base)
    frontier = set(base)
    for _ in range(depth):
        next_frontier: set[Expr] = set()
        for expr in frontier:
            for constructor in CONSTRUCTORS:
                _add(next_frontier, constructor(expr))
            for extent in (2, 3):
                for constructor in ARRAY_CONSTRUCTORS:
                    _add(next_frontier, constructor(expr, extent))
            _add(next_frontier, variant(expr, leaf("long")))
        forms |= next_frontier
        frontier = next_frontier
    return tuple(sorted(forms, key=Expr.render))


@lru_cache(maxsize=None)
def projected_requests(
    storage: StoragePath, depth: int = DEPTH
) -> tuple[tuple[Binding, Expr], ...]:
    projections: list[tuple[Binding, Expr]] = []
    for request in object_requests(storage, depth):
        projections.append((CONCRETE, request))
        projections.append((ANNOTATED, annotated(request, ANNOTATED.tag)))
        projections.append((KEYED, keyed(request, KEYED.key)))
        projections.append((INTERFACE, interface(request, INTERFACE.interfaces[0])))
        projections.append(
            (
                ANNOTATED_INTERFACE,
                annotated(
                    interface(request, INTERFACE.interfaces[0]),
                    ANNOTATED_INTERFACE.tag,
                ),
            )
        )
    return tuple(projections)


@lru_cache(maxsize=None)
def request_forms(depth: int = DEPTH) -> tuple[Expr, ...]:
    forms: set[Expr] = set()
    for storage in STORAGE_PATHS:
        forms.update(request for _, request in projected_requests(storage, depth))
    return tuple(sorted(forms, key=Expr.render))
