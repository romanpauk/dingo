#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The one type grammar and its bounded projections.

Every type the model talks about is produced here from a leaf. The storage and
request axes are projections of the same constructor set and bounds, so a
missing spelling is a grammar decision, not a forgotten list in a consumer.
Consumers choose a projection and filter it; they do not enumerate types of
their own.
"""

from __future__ import annotations

from functools import lru_cache
from itertools import combinations

from spec.model.grammar.expr import (
    Expr,
    annotated,
    array,
    const,
    constructors as constructor_specs,
    interface,
    keyed,
    leaf,
    lvalue_reference,
    pointer,
    valid,
    variant,
)

LEAF = leaf("T")
EXTENTS = (2, 3)
QUALIFIER_DEPTH = 2
WRAPPER_DEPTH = 2


def _constructors(kind: str) -> tuple[object, ...]:
    return tuple(spec.make for spec in constructor_specs(kind))


QUALIFIERS = _constructors("qualifier")
WRAPPERS = _constructors("wrapper")
ARRAY_CONSTRUCTORS = _constructors("array")
VARIANT_ALTERNATIVE = leaf("long")


def _add(target: set[Expr], expr: Expr | None) -> bool:
    if expr is None or not valid(expr) or expr in target:
        return False
    target.add(expr)
    return True


@lru_cache(maxsize=None)
def plain_forms(
    leaf_expr: Expr = LEAF,
    extents: tuple[int, ...] = EXTENTS,
    depth: int = QUALIFIER_DEPTH,
) -> tuple[Expr, ...]:
    forms = {leaf_expr}
    frontier = {leaf_expr}
    for _ in range(depth):
        next_frontier: set[Expr] = set()
        for expr in frontier:
            for qualifier in QUALIFIERS:
                _add(next_frontier, qualifier(expr))
            for extent in extents:
                for array_constructor in ARRAY_CONSTRUCTORS:
                    _add(next_frontier, array_constructor(expr, extent))
        forms |= next_frontier
        frontier = next_frontier
    return tuple(sorted(forms, key=Expr.render))


@lru_cache(maxsize=None)
def wrapped_forms(
    base: tuple[Expr, ...],
    depth: int = WRAPPER_DEPTH,
) -> tuple[Expr, ...]:
    current = set(base)
    wrapped: set[Expr] = set()
    for _ in range(depth):
        next_frontier: set[Expr] = set()
        for expr in current:
            for wrapper in WRAPPERS:
                _add(next_frontier, wrapper(expr))
        wrapped |= next_frontier
        current = next_frontier
    return tuple(sorted(wrapped, key=Expr.render))


@lru_cache(maxsize=None)
def variant_forms(pool: tuple[Expr, ...]) -> tuple[Expr, ...]:
    forms = set()
    for first, second in combinations(pool, 2):
        candidate = variant(first, second)
        if candidate is not None:
            forms.add(candidate)
    return tuple(sorted(forms, key=Expr.render))


@lru_cache(maxsize=None)
def annotated_forms(forms: tuple[Expr, ...]) -> tuple[Expr, ...]:
    return tuple(sorted({annotated(expr) for expr in forms}, key=Expr.render))


def keyed_forms(forms: tuple[Expr, ...]) -> tuple[Expr, ...]:
    return tuple(sorted({keyed(expr) for expr in forms}, key=Expr.render))


def interface_forms(forms: tuple[Expr, ...]) -> tuple[Expr, ...]:
    return tuple(sorted({interface(expr) for expr in forms}, key=Expr.render))


def variant_pool(wrapped: tuple[Expr, ...]) -> tuple[Expr, ...]:
    """Object alternatives used for bounded variant paths."""
    wanted = {
        LEAF,
        VARIANT_ALTERNATIVE,
        const(LEAF),
        pointer(LEAF),
        const(pointer(LEAF)),
    }
    simple = {LEAF, const(LEAF), pointer(LEAF)}
    for expr in wrapped:
        if expr.op not in {
            "optional",
            "unique_ptr",
            "shared_ptr",
            "unique_array",
            "shared_array",
        }:
            continue
        if expr.inner in simple:
            wanted.add(expr)
    return tuple(
        sorted({expr for expr in wanted if expr and valid(expr)}, key=Expr.render)
    )


@lru_cache(maxsize=None)
def qualified_forms(
    base: tuple[Expr, ...],
    depth: int = QUALIFIER_DEPTH - 1,
    extents: tuple[int, ...] = EXTENTS,
) -> tuple[Expr, ...]:
    """Access/qualifier closure over composed object forms.

    A wrapper or a variant already spends one constructor level, so the
    remaining qualifier budget is one. This is what makes ``optional<T> &``,
    ``shared_ptr<T> *``, and ``variant<const T, long> &`` part of the same
    universe as the plain paths.
    """
    forms: set[Expr] = set()
    frontier = set(base)
    for _ in range(depth):
        next_frontier: set[Expr] = set()
        for expr in frontier:
            for qualifier in QUALIFIERS:
                _add(next_frontier, qualifier(expr))
            for extent in extents:
                for array_constructor in ARRAY_CONSTRUCTORS:
                    _add(next_frontier, array_constructor(expr, extent))
        forms |= next_frontier
        frontier = next_frontier
    return tuple(sorted(forms, key=Expr.render))


@lru_cache(maxsize=None)
def object_forms(
    depth: int = QUALIFIER_DEPTH,
    extents: tuple[int, ...] = EXTENTS,
) -> tuple[Expr, ...]:
    """Every object/request type, before binding qualifiers are applied."""
    plain = plain_forms(extents=extents, depth=depth)
    wrapped = wrapped_forms(plain)
    variants = variant_forms(variant_pool(wrapped))
    composed = set(wrapped) | set(variants)
    access = qualified_forms(tuple(composed), extents=extents)
    return tuple(sorted(set(plain) | composed | set(access), key=Expr.render))


@lru_cache(maxsize=None)
def request_types(
    depth: int = QUALIFIER_DEPTH,
    extents: tuple[int, ...] = EXTENTS,
) -> tuple[Expr, ...]:
    """The request projection: objects plus binding qualifiers."""
    base = object_forms(depth=depth, extents=extents)
    forms = (
        set(base)
        | set(annotated_forms(base))
        | set(keyed_forms(base))
        | set(interface_forms(base))
    )
    return tuple(sorted(forms, key=Expr.render))


@lru_cache(maxsize=None)
def storage_types(
    leaf_expr: Expr = LEAF,
    extents: tuple[int, ...] = EXTENTS,
) -> tuple[Expr, ...]:
    """The storage projection: the object shapes a scope can hold.

    Storage is shallower than requests by construction. A base is a leaf or a
    const leaf; pointers, arrays, wrappers, and a two-alternative variant are
    applied once, and an lvalue reference is the external-only borrow spelling.
    """
    exprs: set[Expr] = set()
    bases = (leaf_expr, const(leaf_expr))
    for base in bases:
        _add(exprs, base)
        _add(exprs, pointer(base))
        _add(exprs, const(pointer(base)))
        for extent in extents:
            _add(exprs, array(base, extent))
            _add(exprs, array(pointer(base), extent))
        _add(exprs, array(array(base, 2), 3))
        for wrapper in WRAPPERS:
            _add(exprs, wrapper(base))
        _add(exprs, variant(base, VARIANT_ALTERNATIVE))
    _add(exprs, lvalue_reference(leaf_expr))
    return tuple(sorted(exprs, key=Expr.render))
