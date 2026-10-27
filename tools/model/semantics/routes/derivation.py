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

from tools.model.grammar.catalog import FORM_BY_NAME, SCOPE_BY_NAME
from tools.model.grammar.shapes import (
    BASE,
    WRAPPER_BY_NAME,
    Shape,
    WrapperSemantics,
)


@dataclass(frozen=True)
class CandidateSpec:
    """The kind and retention a derivation rule declares."""

    kind: str
    retention: bool = False


@dataclass(frozen=True)
class RouteCandidate:
    """One published route and its derived metadata."""

    kind: str
    retention: bool
    steps: tuple[str, ...]
    # The exact request shape the route resolves. Consumers use this instead of
    # re-deriving the request type from the target label.
    shape: Shape
    target: str


def _candidate(kind: str, retention: bool = False) -> CandidateSpec:
    return CandidateSpec(kind, retention)



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
    candidates: tuple[CandidateSpec, ...] = ()

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
    candidates: tuple[CandidateSpec, ...] = (),
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
    return FORM_BY_NAME[form_name].wrapper


def form_kind(form_name: str) -> str:
    return FORM_BY_NAME[form_name].kind


_IN_PLACE_KINDS = frozenset({"handle", "array_handle", "optional"})


@dataclass(frozen=True)
class StepRule:
    """The internal conversion steps of one published candidate."""

    category: str
    candidate: str
    form: str | None
    source: str | None
    steps: tuple[str, ...]

    def matches(
        self, category: str, candidate: str, form: str, source: str
    ) -> bool:
        return (
            self.category == category
            and self.candidate == candidate
            and (self.form is None or self.form == form)
            and (self.source is None or self.source == source)
        )


_SIMPLE_STEPS = frozenset(
    {
        "traits",
        "identity",
        "reference",
        "address",
        "array_reference",
        "array_pointer",
        "pointer_access",
        "alternative",
    }
)

# Internal steps are the conversion nodes in execution order. Most candidates
# are one leaf conversion; dereference, borrow, and wrapper candidates compose
# a leaf underneath their outer step.
STEP_RULES = (
    StepRule(
        "value", "dereference", "variant", None,
        ("dereference", "alternative"),
    ),
    StepRule(
        "object", "dereference", "variant", None,
        ("dereference", "alternative"),
    ),
    StepRule(
        "object", "dereference", "pointer", "cyclical_double",
        ("dereference", "dereference", "traits"),
    ),
    StepRule("value", "dereference", None, None, ("dereference", "identity")),
    StepRule(
        "object", "borrowed", "variant", None, ("borrowed", "alternative")
    ),
    StepRule("value", "borrowed", None, None, ("borrowed", "identity")),
    StepRule("object", "borrowed", None, None, ("borrowed", "traits")),
    StepRule("value", "wrapper", None, None, ("identity", "wrapper")),
    StepRule("rvalue", "wrapper", None, None, ("identity", "wrapper")),
)


def _candidate_steps(
    category: str, candidate: str, form: str, source: str
) -> tuple[str, ...]:
    for rule in STEP_RULES:
        if rule.matches(category, candidate, form, source):
            return rule.steps
    if candidate in _SIMPLE_STEPS:
        return (candidate,)
    raise KeyError(
        f"no {category} step rule for {candidate} from {form}/{source}"
    )


def route_candidate_steps(
    scope_name: str, form_name: str, category: str
) -> tuple[tuple[str, ...], ...]:
    return tuple(
        candidate.steps
        for candidate in route_candidates(scope_name, form_name, category)
    )


TARGET_SHAPES = frozenset(
    {
        "value",
        "lvalue_reference",
        "rvalue_reference",
        "pointer",
        "array",
        "array_pointer",
        "wrapper",
        "wrapper_pointer",
    }
)


_LVALUE = WRAPPER_BY_NAME["lvalue_reference"]
_RVALUE = WRAPPER_BY_NAME["rvalue_reference"]
_POINTER = WRAPPER_BY_NAME["pointer"]
_ARRAY_REFERENCE = WRAPPER_BY_NAME["array_lvalue_reference"]
_ARRAY_POINTER = WRAPPER_BY_NAME["array_pointer"]


def _stored_shape(form_name: str) -> Shape:
    stored = FORM_BY_NAME[form_name].wrapper
    return BASE if stored is None else BASE.apply(stored)


def _candidate_target_shape(
    category: str,
    candidate: str,
    form_name: str,
    form: str,
    source: str,
    position: int,
) -> tuple[str, Shape]:
    """The target label and the exact request shape it denotes."""
    stored = _stored_shape(form_name)
    leaf_reference = BASE.apply(_LVALUE)
    leaf_pointer = BASE.apply(_POINTER)

    if category == "value":
        if candidate in {"dereference", "borrowed", "alternative"}:
            return "value", BASE
        if candidate == "wrapper":
            return "wrapper", stored
        if form == "value":
            return "value", BASE
        return "wrapper", stored
    if category == "rvalue":
        return "rvalue_reference", BASE.apply(_RVALUE)
    if candidate == "pointer_access":
        return "pointer", leaf_pointer
    if candidate == "array_reference":
        return "lvalue_reference", BASE.apply(_ARRAY_REFERENCE)
    if candidate == "array_pointer":
        return "array_pointer", BASE.apply(_ARRAY_POINTER)
    if candidate == "address":
        if form == "value":
            return "pointer", leaf_pointer
        return "wrapper_pointer", stored.apply(_POINTER)
    if candidate == "borrowed":
        if form in {"handle", "optional"}:
            return "lvalue_reference", stored.apply(_LVALUE)
        return "pointer", leaf_pointer
    if candidate == "dereference":
        return "lvalue_reference", leaf_reference
    if candidate == "alternative":
        # An object alternative publishes the value route first and the
        # address route second.
        if position == 0:
            return "lvalue_reference", leaf_reference
        return "pointer", leaf_pointer
    if candidate == "traits":
        if form in {"pointer", "array"} or source == "pointer":
            return "pointer", leaf_pointer
        return "lvalue_reference", leaf_reference
    raise KeyError(
        f"no {category} target rule for {candidate} from {form}/{source}"
    )


def route_candidate_targets(
    scope_name: str, form_name: str, category: str
) -> tuple[str, ...]:
    return tuple(
        candidate.target
        for candidate in route_candidates(scope_name, form_name, category)
    )


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
) -> tuple[RouteCandidate, ...]:
    source = source_shape(scope_name, form_name)
    form = form_kind(form_name)
    stored = wrapper(form_name)
    for rule in DERIVATION_RULES:
        if rule.matches(category, form, source, stored):
            candidates = []
            for index, spec in enumerate(rule.candidates):
                target, shape = _candidate_target_shape(
                    category,
                    spec.kind,
                    form_name,
                    form,
                    source,
                    index,
                )
                candidates.append(
                    RouteCandidate(
                        spec.kind,
                        spec.retention,
                        _candidate_steps(category, spec.kind, form, source),
                        shape,
                        target,
                    )
                )
            return tuple(candidates)
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
