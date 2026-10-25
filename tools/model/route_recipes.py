#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Derive the operation kind and retention sequence of a storage cell route.

A cell route is a short semantic script: how the scope hands out the stored
shape (pointer, lvalue, rvalue, or a cyclical pointer) and what the request
does with it (dereference, borrow, take, wrap, select an alternative, view an
array). The scripts are keyed by storage form, not by scope/form cell, so a new
scope uses the same form scripts and a new form adds one script instead of a
row for every scope.

The C++ route asserts remain the oracle. A supported cell with no script fails
loudly here instead of silently losing its operation-kind coverage.
"""

from __future__ import annotations

from dataclasses import dataclass

from storage_cells import SCOPE_BY_NAME
from wrapper_semantics import FORM_WRAPPERS, WrapperSemantics


@dataclass(frozen=True)
class RouteStep:
    kind: str
    retention: bool = False


def _step(kind: str, retention: bool = False) -> RouteStep:
    return RouteStep(kind, retention)


# Pointer-like wrappers are stored in place, so shared and external hand them
# out as lvalues. The value forms and raw pointers are handed out as addresses
# by shared storage.
_IN_PLACE_FORMS = frozenset(
    {
        "optional",
        "unique_handle",
        "unique_array_handle",
        "shared_handle",
        "shared_array_handle",
    }
)

_PROMOTED_HANDLE = (_step("traits", True),) * 4
_SOLE_OWNER_VALUE = (
    _step("identity"),
    _step("traits", True),
    _step("traits", True),
    _step("traits", True),
)
_SHARED_VALUE = (
    _step("borrowed", True),
    _step("traits", True),
    _step("identity"),
)
_SHARED_RVALUE = (_step("identity"), _step("traits", True))
_ARRAY_SHARED_VALUE = (_step("identity"), _step("traits", True))


def wrapper(form_name: str) -> WrapperSemantics | None:
    return FORM_WRAPPERS.get(form_name)


def source_shape(scope_name: str, form_name: str) -> str:
    """How one cell hands its stored shape to a conversion.

    ``pointer``/``lvalue``/``rvalue`` name the value category of the source.
    ``cyclical_double`` is the pointer-to-pointer a cyclical pointer cell
    stores, and ``lvalue_cyclical`` is the stable shared handle a cyclical cell
    publishes instead of its leaf.
    """
    scope = SCOPE_BY_NAME[scope_name]
    if scope.consumes:
        return "rvalue"
    if scope.allows_cycles:
        if form_name == "pointer":
            return "cyclical_double"
        if form_name in _IN_PLACE_FORMS:
            return "lvalue_cyclical"
        return "pointer"
    # shared and external differ in what their storage returns for a value
    # form: an address from shared, an lvalue reference from external.
    if form_name in _IN_PLACE_FORMS:
        return "lvalue"
    return "pointer" if scope.returns_pointer else "lvalue"


def _value_steps(form_name: str, source: str) -> tuple[RouteStep, ...]:
    stored = wrapper(form_name)
    if form_name == "plain":
        if source == "pointer":
            return (_step("dereference"),)
        if source == "lvalue":
            return (_step("identity"),)
        if source == "rvalue":
            return (_step("identity"), _step("wrapper", True))
    if form_name == "pointer":
        if source == "rvalue":
            return _PROMOTED_HANDLE
        return (_step("dereference"),)
    if form_name == "bounded_array":
        if source == "rvalue":
            return _PROMOTED_HANDLE
        return ()
    if stored is not None and stored.kind == "variant":
        if source == "pointer":
            return (_step("dereference", True),)
        if source in {"lvalue", "rvalue"}:
            return (_step("alternative", True),)
    if form_name == "unique_handle":
        if source == "lvalue":
            return (_step("borrowed", True),)
        if source == "rvalue":
            return _SOLE_OWNER_VALUE
    if form_name == "unique_array_handle":
        if source == "rvalue":
            return _SOLE_OWNER_VALUE
        return ()
    if form_name == "shared_handle":
        if source == "lvalue":
            return _SHARED_VALUE
        if source == "lvalue_cyclical":
            return _ARRAY_SHARED_VALUE
        if source == "rvalue":
            return _SHARED_RVALUE
    if form_name == "shared_array_handle":
        if source in {"lvalue", "rvalue"}:
            return _ARRAY_SHARED_VALUE
    if form_name == "optional":
        if source == "lvalue":
            return (_step("borrowed", True), _step("identity"))
        if source == "rvalue":
            return (_step("identity"),)
    raise KeyError(f"no value route script for {form_name} from {source}")


def _object_steps(form_name: str, source: str) -> tuple[RouteStep, ...]:
    stored = wrapper(form_name)
    if form_name == "plain":
        if source == "lvalue":
            return (_step("address"),)
        return (_step("traits"),)
    if form_name == "pointer":
        if source == "cyclical_double":
            return (_step("dereference", True), _step("borrowed", True))
        return (_step("traits"),)
    if form_name == "bounded_array":
        if source == "rvalue":
            return (_step("traits"), _step("array_pointer", True))
        return (
            _step("array_reference", True),
            _step("traits"),
            _step("array_pointer", True),
        )
    if stored is not None and stored.kind == "variant":
        if source == "pointer":
            return (_step("dereference", True), _step("borrowed", True))
        if source == "lvalue":
            return (_step("alternative", True), _step("alternative", True))
        return ()
    if form_name in {"unique_handle", "shared_handle"}:
        if source in {"lvalue", "lvalue_cyclical"}:
            return (
                _step("address"),
                _step("borrowed", True),
                _step("pointer_access", True),
            )
        return ()
    if form_name in {"unique_array_handle", "shared_array_handle"}:
        if source in {"lvalue", "lvalue_cyclical"}:
            return (_step("address"), _step("pointer_access", True))
        return ()
    if form_name == "optional":
        if source == "lvalue":
            return (
                _step("borrowed", True),
                _step("traits"),
                _step("pointer_access", True),
                _step("address"),
            )
        return ()
    raise KeyError(f"no object route script for {form_name} from {source}")


def _rvalue_steps(form_name: str, source: str) -> tuple[RouteStep, ...]:
    if source != "rvalue":
        raise KeyError(f"no rvalue route script for {form_name} from {source}")
    if form_name == "plain":
        return (_step("identity"), _step("wrapper", True))
    if form_name == "alternative":
        return (_step("alternative", True),)
    if form_name in {"pointer", "bounded_array"}:
        return _PROMOTED_HANDLE
    if form_name in {"unique_handle", "unique_array_handle"}:
        return _SOLE_OWNER_VALUE
    if form_name in {"shared_handle", "shared_array_handle"}:
        return _SHARED_RVALUE
    if form_name == "optional":
        return (_step("identity"),)
    raise KeyError(f"no rvalue route script for {form_name}")


def route_steps(
    scope_name: str, form_name: str, category: str
) -> tuple[RouteStep, ...]:
    source = source_shape(scope_name, form_name)
    if category == "value":
        return _value_steps(form_name, source)
    if category == "object":
        return _object_steps(form_name, source)
    if category == "rvalue":
        return _rvalue_steps(form_name, source)
    raise KeyError(f"unknown route category {category!r}")


def route_kinds(
    scope_name: str, form_name: str, category: str
) -> tuple[str, ...]:
    return tuple(
        step.kind for step in route_steps(scope_name, form_name, category)
    )


def route_retention(
    scope_name: str, form_name: str, category: str
) -> tuple[int, ...]:
    return tuple(
        int(step.retention)
        for step in route_steps(scope_name, form_name, category)
    )


def interface_route_counts(
    scope_name: str, form_name: str, *, copyable_value: bool = False
) -> dict[str, int]:
    """Route counts for storage exposed through a base interface.

    The interface route model is smaller than the leaf model: a base interface
    adds an upcast to the same wrapper routes. A shared handle publishes the
    interface wrapper and its const copy, and exposes the interface reference
    and pointer through the wrapper. A unique handle consumes into its owning
    handles. A plain value keeps only the object route to the interface.
    """
    if scope_name == "shared" and form_name == "shared_handle":
        return {
            "value": 2 + int(copyable_value),
            "object": 4,
            "rvalue": 0,
        }
    if scope_name == "unique" and form_name == "unique_handle":
        return {"value": 4, "object": 0, "rvalue": 4}
    if scope_name in {"shared", "external"} and form_name == "plain":
        return {"value": 0, "object": 1, "rvalue": 0}
    raise KeyError(
        f"no interface route script for {scope_name}/{form_name}"
    )

