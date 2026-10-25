#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Derive a cell route from wrapper properties and shape categories.

A route is selected by matching (category, form kind, source shape, wrapper
properties) against an ordered rule list. The rules do not name a storage form:
a custom wrapper with the same kind and properties follows the same rule as the
built-in wrapper it mirrors. The generated C++ route asserts remain the oracle.
"""

from __future__ import annotations

from dataclasses import dataclass

from storage_cells import FORM_BY_NAME, SCOPE_BY_NAME
from wrapper_semantics import FORM_WRAPPERS, WrapperSemantics


@dataclass(frozen=True)
class RouteStep:
    kind: str
    retention: bool = False


def _step(kind: str, retention: bool = False) -> RouteStep:
    return RouteStep(kind, retention)



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


@dataclass(frozen=True)
class Rule:
    category: str
    kind: str
    source: str | None
    requires: frozenset[str]
    steps: tuple[RouteStep, ...] = ()

    def matches(
        self,
        category: str,
        kind: str,
        source: str,
        wrapper: WrapperSemantics | None,
    ) -> bool:
        if self.category != category or self.kind != kind:
            return False
        if self.source is not None and self.source != source:
            return False
        if self.requires and (
            wrapper is None or not self.requires <= wrapper.properties
        ):
            return False
        return True


def _rule(
    category: str,
    kind: str,
    source: str | None,
    requires: set[str] | frozenset[str] = frozenset(),
    steps: tuple[RouteStep, ...] = (),
) -> Rule:
    return Rule(category, kind, source, frozenset(requires), steps)


# Ordered rules. Specific source/ownership cases come before wildcards; the
# first match wins. The sequences are the same ones the generated C++ route
# asserts check.
RULES = (
    # Value requests.
    _rule("value", "value", "pointer", steps=(_step("dereference"),)),
    _rule("value", "value", "lvalue", steps=(_step("identity"),)),
    _rule(
        "value",
        "value",
        "rvalue",
        steps=(_step("identity"), _step("wrapper", True)),
    ),
    _rule("value", "pointer", "rvalue", steps=_PROMOTED_HANDLE),
    _rule("value", "pointer", "pointer", steps=(_step("dereference"),)),
    _rule("value", "pointer", "lvalue", steps=(_step("dereference"),)),
    _rule("value", "array", "rvalue", steps=_PROMOTED_HANDLE),
    _rule("value", "array", "pointer"),
    _rule("value", "variant", "pointer", steps=(_step("dereference", True),)),
    _rule("value", "variant", "lvalue", steps=(_step("alternative", True),)),
    _rule("value", "variant", "rvalue", steps=(_step("alternative", True),)),
    _rule(
        "value",
        "handle",
        "lvalue",
        requires={"sharing"},
        steps=_SHARED_VALUE,
    ),
    _rule(
        "value",
        "handle",
        "lvalue",
        requires={"sole_owner"},
        steps=(_step("borrowed", True),),
    ),
    _rule(
        "value",
        "handle",
        "lvalue_cyclical",
        requires={"sharing"},
        steps=_ARRAY_SHARED_VALUE,
    ),
    _rule(
        "value",
        "handle",
        "rvalue",
        requires={"sharing"},
        steps=_SHARED_RVALUE,
    ),
    _rule(
        "value",
        "handle",
        "rvalue",
        requires={"sole_owner"},
        steps=_SOLE_OWNER_VALUE,
    ),
    _rule(
        "value",
        "array_handle",
        "lvalue",
        requires={"sharing"},
        steps=_ARRAY_SHARED_VALUE,
    ),
    _rule("value", "array_handle", "lvalue", requires={"sole_owner"}),
    _rule(
        "value",
        "array_handle",
        "rvalue",
        requires={"sharing"},
        steps=_ARRAY_SHARED_VALUE,
    ),
    _rule(
        "value",
        "array_handle",
        "rvalue",
        requires={"sole_owner"},
        steps=_SOLE_OWNER_VALUE,
    ),
    _rule(
        "value",
        "optional",
        "lvalue",
        steps=(_step("borrowed", True), _step("identity")),
    ),
    _rule("value", "optional", "rvalue", steps=(_step("identity"),)),
    # Object requests.
    _rule("object", "value", "lvalue", steps=(_step("address"),)),
    _rule("object", "value", None, steps=(_step("traits"),)),
    _rule(
        "object",
        "pointer",
        "cyclical_double",
        steps=(_step("dereference", True), _step("borrowed", True)),
    ),
    _rule("object", "pointer", None, steps=(_step("traits"),)),
    _rule(
        "object",
        "array",
        "rvalue",
        steps=(_step("traits"), _step("array_pointer", True)),
    ),
    _rule(
        "object",
        "array",
        None,
        steps=(
            _step("array_reference", True),
            _step("traits"),
            _step("array_pointer", True),
        ),
    ),
    _rule(
        "object",
        "variant",
        "pointer",
        steps=(_step("dereference", True), _step("borrowed", True)),
    ),
    _rule(
        "object",
        "variant",
        "lvalue",
        steps=(_step("alternative", True), _step("alternative", True)),
    ),
    _rule("object", "variant", "rvalue"),
    _rule(
        "object",
        "handle",
        "lvalue",
        steps=(
            _step("address"),
            _step("borrowed", True),
            _step("pointer_access", True),
        ),
    ),
    _rule(
        "object",
        "handle",
        "lvalue_cyclical",
        steps=(
            _step("address"),
            _step("borrowed", True),
            _step("pointer_access", True),
        ),
    ),
    _rule("object", "handle", None),
    _rule(
        "object",
        "array_handle",
        "lvalue",
        steps=(_step("address"), _step("pointer_access", True)),
    ),
    _rule(
        "object",
        "array_handle",
        "lvalue_cyclical",
        steps=(_step("address"), _step("pointer_access", True)),
    ),
    _rule("object", "array_handle", None),
    _rule(
        "object",
        "optional",
        "lvalue",
        steps=(
            _step("borrowed", True),
            _step("traits"),
            _step("pointer_access", True),
            _step("address"),
        ),
    ),
    _rule("object", "optional", None),
    # Rvalue requests.
    _rule(
        "rvalue",
        "value",
        None,
        steps=(_step("identity"), _step("wrapper", True)),
    ),
    _rule("rvalue", "variant", None, steps=(_step("alternative", True),)),
    _rule("rvalue", "pointer", None, steps=_PROMOTED_HANDLE),
    _rule("rvalue", "array", None, steps=_PROMOTED_HANDLE),
    _rule(
        "rvalue",
        "handle",
        None,
        requires={"sharing"},
        steps=_SHARED_RVALUE,
    ),
    _rule(
        "rvalue",
        "handle",
        None,
        requires={"sole_owner"},
        steps=_SOLE_OWNER_VALUE,
    ),
    _rule(
        "rvalue",
        "array_handle",
        None,
        requires={"sharing"},
        steps=_SHARED_RVALUE,
    ),
    _rule(
        "rvalue",
        "array_handle",
        None,
        requires={"sole_owner"},
        steps=_SOLE_OWNER_VALUE,
    ),
    _rule("rvalue", "optional", None, steps=(_step("identity"),)),
)


def wrapper(form_name: str) -> WrapperSemantics | None:
    return FORM_WRAPPERS.get(form_name)


def form_kind(form_name: str) -> str:
    return FORM_BY_NAME[form_name].kind


_IN_PLACE_KINDS = frozenset({"handle", "array_handle", "optional"})


def source_shape(scope_name: str, form_name: str) -> str:
    """How one cell hands its stored shape to a conversion."""
    scope = SCOPE_BY_NAME[scope_name]
    kind = form_kind(form_name)
    if scope.consumes:
        return "rvalue"
    if scope.allows_cycles:
        if kind == "pointer":
            return "cyclical_double"
        if kind in _IN_PLACE_KINDS:
            return "lvalue_cyclical"
        return "pointer"
    if kind in _IN_PLACE_KINDS:
        return "lvalue"
    return "pointer" if scope.returns_pointer else "lvalue"


def route_steps(
    scope_name: str, form_name: str, category: str
) -> tuple[RouteStep, ...]:
    source = source_shape(scope_name, form_name)
    kind = form_kind(form_name)
    stored = wrapper(form_name)
    for rule in RULES:
        if rule.matches(category, kind, source, stored):
            return rule.steps
    raise KeyError(
        f"no {category} route rule for {scope_name}/{form_name} from {source}"
    )


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
    """Route counts for storage exposed through a base interface."""
    kind = form_kind(form_name)
    stored = wrapper(form_name)
    if scope_name == "shared" and kind == "handle" and stored is not None:
        if stored.has("sharing"):
            return {
                "value": 2 + int(copyable_value),
                "object": 4,
                "rvalue": 0,
            }
    if scope_name == "unique" and kind == "handle":
        return {"value": 4, "object": 0, "rvalue": 4}
    if scope_name in {"shared", "external"} and kind == "value":
        return {"value": 0, "object": 1, "rvalue": 0}
    raise KeyError(
        f"no interface route rule for {scope_name}/{form_name}"
    )
