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
class ResolutionCandidate:
    kind: str
    retention: bool = False


def _candidate(kind: str, retention: bool = False) -> ResolutionCandidate:
    return ResolutionCandidate(kind, retention)



_PROMOTED_HANDLE = (_candidate("traits", True),) * 4
_SOLE_OWNER_VALUE = (
    _candidate("identity"),
    _candidate("traits", True),
    _candidate("traits", True),
    _candidate("traits", True),
)
_SHARED_VALUE = (
    _candidate("borrowed", True),
    _candidate("traits", True),
    _candidate("identity"),
)
_SHARED_RVALUE = (_candidate("identity"), _candidate("traits", True))
_ARRAY_SHARED_VALUE = (_candidate("identity"), _candidate("traits", True))


@dataclass(frozen=True)
class Rule:
    category: str
    kind: str
    source: str | None
    requires: frozenset[str]
    candidates: tuple[ResolutionCandidate, ...] = ()

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
    candidates: tuple[ResolutionCandidate, ...] = (),
) -> Rule:
    return Rule(category, kind, source, frozenset(requires), candidates)


# Ordered rules. Specific source/ownership cases come before wildcards; the
# first match wins. The sequences are the same ones the generated C++ route
# asserts check.
DERIVATION_RULES = (
    # Value requests.
    _rule("value", "value", "pointer", candidates=(_candidate("dereference"),)),
    _rule("value", "value", "lvalue", candidates=(_candidate("identity"),)),
    _rule(
        "value",
        "value",
        "rvalue",
        candidates=(_candidate("identity"), _candidate("wrapper", True)),
    ),
    _rule("value", "pointer", "rvalue", candidates=_PROMOTED_HANDLE),
    _rule("value", "pointer", "pointer", candidates=(_candidate("dereference"),)),
    _rule("value", "pointer", "lvalue", candidates=(_candidate("dereference"),)),
    _rule("value", "array", "rvalue", candidates=_PROMOTED_HANDLE),
    _rule("value", "array", "pointer"),
    _rule("value", "variant", "pointer", candidates=(_candidate("dereference", True),)),
    _rule("value", "variant", "lvalue", candidates=(_candidate("alternative", True),)),
    _rule("value", "variant", "rvalue", candidates=(_candidate("alternative", True),)),
    _rule(
        "value",
        "handle",
        "lvalue",
        requires={"sharing"},
        candidates=_SHARED_VALUE,
    ),
    _rule(
        "value",
        "handle",
        "lvalue",
        requires={"sole_owner"},
        candidates=(_candidate("borrowed", True),),
    ),
    _rule(
        "value",
        "handle",
        "lvalue_cyclical",
        requires={"sharing"},
        candidates=_ARRAY_SHARED_VALUE,
    ),
    _rule(
        "value",
        "handle",
        "rvalue",
        requires={"sharing"},
        candidates=_SHARED_RVALUE,
    ),
    _rule(
        "value",
        "handle",
        "rvalue",
        requires={"sole_owner"},
        candidates=_SOLE_OWNER_VALUE,
    ),
    _rule(
        "value",
        "array_handle",
        "lvalue",
        requires={"sharing"},
        candidates=_ARRAY_SHARED_VALUE,
    ),
    _rule("value", "array_handle", "lvalue", requires={"sole_owner"}),
    _rule(
        "value",
        "array_handle",
        "rvalue",
        requires={"sharing"},
        candidates=_ARRAY_SHARED_VALUE,
    ),
    _rule(
        "value",
        "array_handle",
        "rvalue",
        requires={"sole_owner"},
        candidates=_SOLE_OWNER_VALUE,
    ),
    _rule(
        "value",
        "optional",
        "lvalue",
        candidates=(_candidate("borrowed", True), _candidate("identity")),
    ),
    _rule("value", "optional", "rvalue", candidates=(_candidate("identity"),)),
    # Object requests.
    _rule("object", "value", "lvalue", candidates=(_candidate("address"),)),
    _rule("object", "value", None, candidates=(_candidate("traits"),)),
    _rule(
        "object",
        "pointer",
        "cyclical_double",
        candidates=(_candidate("dereference", True), _candidate("borrowed", True)),
    ),
    _rule("object", "pointer", None, candidates=(_candidate("traits"),)),
    _rule(
        "object",
        "array",
        "rvalue",
        candidates=(_candidate("traits"), _candidate("array_pointer", True)),
    ),
    _rule(
        "object",
        "array",
        None,
        candidates=(
            _candidate("array_reference", True),
            _candidate("traits"),
            _candidate("array_pointer", True),
        ),
    ),
    _rule(
        "object",
        "variant",
        "pointer",
        candidates=(_candidate("dereference", True), _candidate("borrowed", True)),
    ),
    _rule(
        "object",
        "variant",
        "lvalue",
        candidates=(_candidate("alternative", True), _candidate("alternative", True)),
    ),
    _rule("object", "variant", "rvalue"),
    _rule(
        "object",
        "handle",
        "lvalue",
        candidates=(
            _candidate("address"),
            _candidate("borrowed", True),
            _candidate("pointer_access", True),
        ),
    ),
    _rule(
        "object",
        "handle",
        "lvalue_cyclical",
        candidates=(
            _candidate("address"),
            _candidate("borrowed", True),
            _candidate("pointer_access", True),
        ),
    ),
    _rule("object", "handle", None),
    _rule(
        "object",
        "array_handle",
        "lvalue",
        candidates=(_candidate("address"), _candidate("pointer_access", True)),
    ),
    _rule(
        "object",
        "array_handle",
        "lvalue_cyclical",
        candidates=(_candidate("address"), _candidate("pointer_access", True)),
    ),
    _rule("object", "array_handle", None),
    _rule(
        "object",
        "optional",
        "lvalue",
        candidates=(
            _candidate("borrowed", True),
            _candidate("traits"),
            _candidate("pointer_access", True),
            _candidate("address"),
        ),
    ),
    _rule("object", "optional", None),
    # Rvalue requests.
    _rule(
        "rvalue",
        "value",
        None,
        candidates=(_candidate("identity"), _candidate("wrapper", True)),
    ),
    _rule("rvalue", "variant", None, candidates=(_candidate("alternative", True),)),
    _rule("rvalue", "pointer", None, candidates=_PROMOTED_HANDLE),
    _rule("rvalue", "array", None, candidates=_PROMOTED_HANDLE),
    _rule(
        "rvalue",
        "handle",
        None,
        requires={"sharing"},
        candidates=_SHARED_RVALUE,
    ),
    _rule(
        "rvalue",
        "handle",
        None,
        requires={"sole_owner"},
        candidates=_SOLE_OWNER_VALUE,
    ),
    _rule(
        "rvalue",
        "array_handle",
        None,
        requires={"sharing"},
        candidates=_SHARED_RVALUE,
    ),
    _rule(
        "rvalue",
        "array_handle",
        None,
        requires={"sole_owner"},
        candidates=_SOLE_OWNER_VALUE,
    ),
    _rule("rvalue", "optional", None, candidates=(_candidate("identity"),)),
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


def route_candidates(
    scope_name: str, form_name: str, category: str
) -> tuple[ResolutionCandidate, ...]:
    source = source_shape(scope_name, form_name)
    kind = form_kind(form_name)
    stored = wrapper(form_name)
    for rule in DERIVATION_RULES:
        if rule.matches(category, kind, source, stored):
            return rule.candidates
    raise KeyError(
        f"no {category} route rule for {scope_name}/{form_name} from {source}"
    )


def route_kinds(
    scope_name: str, form_name: str, category: str
) -> tuple[str, ...]:
    return tuple(
        candidate.kind
        for candidate in route_candidates(scope_name, form_name, category)
    )


def route_retention(
    scope_name: str, form_name: str, category: str
) -> tuple[int, ...]:
    return tuple(
        int(candidate.retention)
        for candidate in route_candidates(scope_name, form_name, category)
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
