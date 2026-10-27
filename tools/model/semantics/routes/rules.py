#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Derive a request route from wrapper semantics and scope exposure.

The rules are a small vocabulary borrowed from the conversion model:

  access, borrow, move, copy_out, take, wrap, share, adopt, decay,
  select_alternative, nested_access, const_copy, target

The library still answers whether a route is published. This module derives the
first rule that blocks a request and names it, so the matrix report says
"take: a borrowing scope cannot become an owner" instead of "nested wrapper".
"""

from __future__ import annotations

from dataclasses import dataclass

from tools.model.grammar.catalog import (
    ARRAY_FORMS,
    ARRAY_HANDLE_FORMS,
    FORM_BY_NAME,
    SCOPE_BY_NAME,
)
from tools.model.semantics.forms import LVALUE, POINTER, RVALUE, VALUE, exposure
from tools.model.grammar.shapes import Shape


@dataclass(frozen=True)
class RouteFailure:
    rule: str
    detail: str

    def text(self) -> str:
        return f"{self.rule}: {self.detail}"


def _fail(rule: str, detail: str) -> RouteFailure:
    return RouteFailure(rule, detail)


def _published(scope_name: str, form_name: str, access: str) -> bool:
    scope = SCOPE_BY_NAME[scope_name]
    form = FORM_BY_NAME[form_name]
    bits = exposure(scope, form)
    if access == "value":
        return (
            form.has_value
            and (bits & VALUE or bits & RVALUE)
        )
    if access == "lvalue_reference":
        return form.has_lvalue and bool(bits & LVALUE)
    if access == "rvalue_reference":
        return bool(bits & RVALUE)
    if access == "pointer":
        return form.has_pointer and bool(bits & POINTER)
    if access == "array_lvalue_reference":
        return form_name == "bounded_array" and bool(bits & LVALUE)
    if access == "array_pointer":
        return form_name == "bounded_array" and bool(bits & POINTER)
    return False


def _leaf_value_published(scope_name: str, form_name: str) -> bool:
    scope = SCOPE_BY_NAME[scope_name]
    form = FORM_BY_NAME[form_name]
    if not form.has_value:
        return False
    bits = exposure(scope, form)
    if bits & VALUE:
        # A leaf is published directly by these forms. An array handle
        # publishes the handle, not its element.
        return form_name in {
            "plain",
            "alternative",
            "pointer",
            "optional",
            "shared_handle",
        }
    if bits & RVALUE:
        # Only the value-like forms publish a leaf rvalue.
        return form_name in {"plain", "alternative"}
    return False


def _can_take(scope_name: str, form_name: str, outer_name: str) -> bool:
    """Whether a consuming scope can produce an owner of the request family."""
    scope = SCOPE_BY_NAME[scope_name]
    if not scope.consumes:
        return False
    family = FORM_BY_NAME[form_name].kind
    if outer_name in {"shared_ptr", "unique_ptr"}:
        return family in {"value", "pointer", "handle", "array"}
    if outer_name in {"shared_array", "unique_array"}:
        return family in {"array", "array_handle"} or form_name in {
            "bounded_array",
            "unique_array_handle",
            "shared_array_handle",
        }
    return False


def _derive_access(
    scope_name: str, form_name: str, shape: Shape, access: str
) -> RouteFailure | None:
    if access == "rvalue_reference":
        if not _published(scope_name, form_name, access):
            return _fail("move", "the scope does not publish an rvalue route")
    elif access == "lvalue_reference":
        if not _published(scope_name, form_name, access):
            return _fail(
                "borrow", "the scope does not publish an lvalue reference"
            )
    elif access == "pointer":
        if not _published(scope_name, form_name, access):
            return _fail("borrow", "the scope does not publish a pointer route")
    elif access in {"array_lvalue_reference", "array_pointer"}:
        if form_name != "bounded_array":
            return _fail("decay", "the cell does not store a bounded array")
        if not _published(scope_name, form_name, access):
            return _fail("decay", "the scope does not publish the array view")

    inner = shape.inner
    if inner is not None and inner.outer is not None and inner.outer.name == "const":
        # const is a qualifier of the object, not a wrapper the cell stores.
        inner = inner.inner
    if inner is not None and inner.access != "value":
        return _fail(
            "nested_access",
            "an access form is nested inside another access form",
        )

    if inner is not None and inner.outer is not None:
        stored = FORM_BY_NAME[form_name].wrapper
        if stored is None or stored.name != inner.outer.name:
            return _fail(
                "borrow",
                f"the cell does not store {inner.outer.name}",
            )

    if (
        access == "lvalue_reference"
        and form_name in ARRAY_FORMS | ARRAY_HANDLE_FORMS
    ):
        return _fail(
            "decay",
            "the cell exposes its row/array, not a leaf reference",
        )
    return None


def _derive_value(
    scope_name: str, form_name: str, shape: Shape
) -> RouteFailure | None:
    outer = shape.outer
    if outer is None:
        if _leaf_value_published(scope_name, form_name):
            return None
        return _fail(
            "copy_out",
            "the cell does not publish its leaf as a value",
        )

    inner = shape.inner
    if inner is None:
        return _fail("target", "the request wrapper has no inner type")

    if inner.access != "value":
        return _fail(
            "nested_access",
            f"{inner.access} is nested inside {outer.name}",
        )

    if inner.outer is not None and inner.outer.is_access:
        return _fail(
            "nested_access",
            f"{inner.outer.name} is nested inside {outer.name}",
        )

    family = FORM_BY_NAME[form_name].kind
    stored = FORM_BY_NAME[form_name].wrapper

    if outer.name == "variant":
        if family != "variant":
            return _fail(
                "select_alternative",
                "the cell does not store a variant",
            )
        if inner.outer is not None:
            return _fail(
                "select_alternative",
                "the cell stores a different alternative",
            )
        return None

    if outer.name == "optional":
        if inner.is_const:
            return _fail(
                "const_copy",
                "const inside an optional has no copy route",
            )
        if inner.outer is None:
            if _leaf_value_published(scope_name, form_name):
                return None
            if family == "optional" and _published(
                scope_name, form_name, "value"
            ):
                return None
            return _fail(
                "copy_out",
                "the cell does not publish the inner value",
            )
        iw = inner.outer
        if iw.has("sharing"):
            if stored is not None and stored.name == iw.name:
                return None
            return _fail(
                "wrap",
                f"the cell does not provide a {iw.name} to wrap",
            )
        if iw.has("sole_owner"):
            if _can_take(scope_name, form_name, outer.name):
                return None
            return _fail(
                "take",
                "a borrowing scope cannot take a sole-owner handle",
            )
        if iw.name in {"optional", "variant"}:
            if family == "optional" or family == "variant":
                return None
            return _fail(
                "wrap",
                f"the cell does not provide a {iw.name} to wrap",
            )
        return _fail("wrap", f"cannot wrap {iw.name}")

    if outer.kind in {"handle", "array_handle"}:
        # A stored handle is the request's whole chain; an inner wrapper means
        # the request wraps something the cell stores inside the handle, not
        # the handle itself.
        stored_match = (
            stored is not None
            and stored.name == outer.name
            and inner.outer is None
        )
        if stored_match:
            if outer.has("sharing") or SCOPE_BY_NAME[scope_name].consumes:
                return None
            return _fail(
                "take",
                f"a borrowing scope cannot take {outer.name}",
            )
        if inner.outer is None:
            if _can_take(scope_name, form_name, outer.name):
                return None
            return _fail(
                "take",
                "a borrowing scope cannot become an owner",
            )
        iw = inner.outer
        if iw.kind in {"handle", "array_handle"}:
            if stored is not None and stored.name == iw.name:
                if iw.has("sharing") or scope_name not in {"shared", "external"}:
                    return None
            if _can_take(scope_name, form_name, outer.name):
                return None
            return _fail(
                "take",
                f"cannot take {iw.name} from a borrowing scope",
            )
        if iw.name in {"optional", "variant"}:
            if _can_take(scope_name, form_name, outer.name):
                return None
            return _fail(
                "take",
                f"cannot take {iw.name} from a borrowing scope",
            )
        return _fail("wrap", f"cannot wrap {iw.name}")

    if outer.name == "const":
        return _fail("const_copy", "a top-level const is not a request")

    return _fail("target", f"no rule derives {outer.name}")


def derive_route(
    scope_name: str, form_name: str, shape: Shape
) -> RouteFailure | None:
    access = shape.access
    if access == "value":
        return _derive_value(scope_name, form_name, shape)
    return _derive_access(scope_name, form_name, shape, access)


def reason_text(scope_name: str, form_name: str, shape: Shape) -> str:
    failure = derive_route(scope_name, form_name, shape)
    if failure is not None:
        return failure.text()
    return "target: the derived route has no published target"
