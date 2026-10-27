#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The request algebra.

A request is a `Shape` plus optional qualifiers. Its resolved facts -- what it
provides, what storage/scope capabilities it requires, and which dependency
form it publishes -- follow from the wrapper chain and the qualifier, not from a
per-row table.
"""

from __future__ import annotations

from dataclasses import dataclass

from tools.model.grammar.shapes import SHAPES, Shape

ALL_MODES = frozenset({"runtime", "static", "mixed"})
SHAPE_BY_NAME = {shape.name: shape for shape in SHAPES}
CONCRETE_ACCESS = frozenset(
    {"stable_concrete_storage", "direct_value_resolution"}
)


@dataclass(frozen=True)
class RequestFacts:
    provides: frozenset[str]
    requires: frozenset[str]
    dependency_forms: frozenset[str]
    supported_modes: frozenset[str] = ALL_MODES


def _facts(
    provides,
    requires=(),
    dependency_forms=(),
    modes=ALL_MODES,
) -> RequestFacts:
    return RequestFacts(
        frozenset(provides),
        frozenset(requires),
        frozenset(dependency_forms),
        modes,
    )


def _leaf_reference(shape: Shape) -> RequestFacts | None:
    inner = shape.inner
    if inner is None:
        return None
    is_const = inner.is_const
    if inner.outer is not None and inner.outer.name == "const":
        inner = inner.inner
    if inner is None or inner.outer is not None:
        return None
    if is_const:
        return _facts(
            {"resolved_concrete", "resolved_const_concrete"},
            CONCRETE_ACCESS,
            {"const_lvalue_reference"},
        )
    return _facts(
        {
            "resolved_concrete",
            "constructable_dependency",
            "invokable_dependency",
        },
        CONCRETE_ACCESS,
        {"lvalue_reference"},
    )


def _leaf_pointer(shape: Shape) -> RequestFacts | None:
    inner = shape.inner
    if inner is None:
        return None
    is_const = inner.is_const
    if inner.outer is not None and inner.outer.name == "const":
        inner = inner.inner
    if inner is None or inner.outer is not None:
        return None
    if is_const:
        return _facts(
            {"resolved_concrete", "resolved_const_concrete"},
            CONCRETE_ACCESS,
            {"const_pointer"},
        )
    return _facts(
        {
            "resolved_concrete",
            "constructable_dependency",
            "invokable_dependency",
        },
        CONCRETE_ACCESS,
        {"pointer"},
    )


def describe(shape: Shape, exposed_kind: str = "concrete") -> RequestFacts | None:
    """Derive one concrete request row from its wrapper chain."""
    if exposed_kind != "concrete":
        return None

    outer = shape.outer
    if outer is None:
        return _facts(
            {"resolved_concrete", "resolved_concrete_value"},
            {"direct_value_resolution"},
            {"value"},
        )
    if outer.name == "rvalue_reference":
        return _facts(
            {"resolved_concrete"},
            {"unique_storage", "stored_value"},
            {"rvalue_reference"},
        )
    if outer.name == "unique_ptr":
        return _facts(
            {"resolved_wrapper"},
            {"unique_storage", "stored_unique_ptr"},
            {"unique_pointer"},
        )
    if outer.name == "shared_ptr":
        return _facts(
            {"resolved_wrapper"},
            {"shared_storage", "stored_shared_ptr"},
            {"shared_pointer"},
        )
    if outer.name == "optional":
        return _facts(
            {"resolved_wrapper"},
            {"unique_storage", "stored_optional"},
            {"optional"},
        )
    if outer.name == "lvalue_reference":
        leaf = _leaf_reference(shape)
        if leaf is not None:
            return leaf
        inner = shape.inner
        if inner is not None and inner.outer is not None:
            if inner.outer.name == "shared_ptr":
                return _facts(
                    {"resolved_wrapper"},
                    {"shared_storage", "stored_shared_ptr"},
                    {"shared_pointer_lvalue_reference"},
                )
            if inner.outer.name == "optional":
                return _facts(
                    {"resolved_wrapper"},
                    {"shared_storage", "stored_optional"},
                    {"optional_lvalue_reference"},
                )
        return None
    if outer.name == "pointer":
        return _leaf_pointer(shape)
    return None


@dataclass(frozen=True)
class ConcreteRequest:
    """A named group of shapes that publish one concrete resolved row."""

    name: str
    shapes: tuple[str, ...]

    def facts(self) -> RequestFacts:
        derived = []
        for shape_name in self.shapes:
            facts = describe(SHAPE_BY_NAME[shape_name])
            if facts is None:
                raise ValueError(f"{shape_name} is not a concrete request")
            derived.append(facts)
        provides = derived[0].provides
        requires = derived[0].requires
        modes = derived[0].supported_modes
        for facts in derived[1:]:
            if facts.provides != provides or facts.requires != requires:
                raise ValueError(f"{self.name} groups shapes with different facts")
            modes = modes & facts.supported_modes
        dependencies = frozenset(
            form for facts in derived for form in facts.dependency_forms
        )
        return RequestFacts(provides, requires, dependencies, modes)


# Only the grouping and the shape members are declared; the facts are derived.
CONCRETE_REQUESTS = (
    ConcreteRequest("value_ref_ptr", ("lvalue_reference_value", "pointer_value")),
    ConcreteRequest("value", ("value",)),
    ConcreteRequest("value_rvalue", ("rvalue_reference_value",)),
    ConcreteRequest("const_value_ref", ("lvalue_reference_const_value",)),
    ConcreteRequest("const_value_ptr", ("pointer_const_value",)),
    ConcreteRequest("value_unique_ptr", ("unique_ptr_value",)),
    ConcreteRequest("value_shared_ptr", ("shared_ptr_value",)),
    ConcreteRequest("value_shared_ptr_ref", ("lvalue_reference_shared_ptr_value",)),
    ConcreteRequest("value_optional", ("optional_value",)),
    ConcreteRequest("value_optional_ref", ("lvalue_reference_optional_value",)),
)


@dataclass(frozen=True)
class AnnotatedRequest:
    """A tagged request over a base shape and an annotated exposure kind."""

    name: str
    base: str
    exposed: str

    def facts(self) -> RequestFacts:
        base = describe(SHAPE_BY_NAME[self.base])
        if base is None:
            raise ValueError(f"{self.base} is not a concrete base request")
        dependency = next(iter(base.dependency_forms))
        if self.exposed == "annotated_concrete":
            requires = {"stable_concrete_storage"}
        else:
            requires = {"shared_storage"}
            if "shared_pointer" in dependency:
                requires.add("stored_shared_ptr")
        return _facts(
            {"resolved_annotated"}, requires, {f"annotated_{dependency}"}
        )


@dataclass(frozen=True)
class KeyedRequest:
    """A keyed request over a base shape and a keyed exposure kind."""

    name: str
    base: str
    exposed: str
    dependency: bool = False

    def facts(self) -> RequestFacts:
        base = describe(SHAPE_BY_NAME[self.base])
        if base is None:
            raise ValueError(f"{self.base} is not a concrete base request")
        if self.exposed == "keyed_interface":
            return _facts({"resolved_keyed"}, {"shared_storage"})
        requires = {"direct_value_resolution"}
        if "lvalue_reference" in base.dependency_forms:
            requires.add("stable_concrete_storage")
        if self.dependency:
            return _facts(
                {"constructable_dependency"},
                requires,
                {"selected_lvalue_reference"},
            )
        return _facts({"resolved_keyed"}, requires)


ANNOTATED_REQUESTS = (
    AnnotatedRequest("annotated_value_ref", "lvalue_reference_value", "annotated_concrete"),
    AnnotatedRequest("annotated_interface_ref", "lvalue_reference_value", "annotated_interface"),
    AnnotatedRequest("annotated_interface_ptr", "pointer_value", "annotated_interface"),
    AnnotatedRequest("annotated_interface_shared_ptr", "shared_ptr_value", "annotated_interface"),
    AnnotatedRequest("annotated_interface_shared_ptr_ref", "lvalue_reference_shared_ptr_value", "annotated_interface"),
)

KEYED_REQUESTS = (
    KeyedRequest("keyed_value", "value", "keyed_concrete"),
    KeyedRequest("keyed_value_ref", "lvalue_reference_value", "keyed_concrete"),
    KeyedRequest("keyed_value_dependency", "lvalue_reference_value", "keyed_concrete", dependency=True),
    KeyedRequest("keyed_interface_ref", "lvalue_reference_value", "keyed_interface"),
)


@dataclass(frozen=True)
class InterfaceRequest:
    """An interface-exposed request with a named capability policy."""

    name: str
    base: str
    exposed: tuple[str, ...]
    policy: str

    def facts(self) -> RequestFacts:
        base = describe(SHAPE_BY_NAME[self.base])
        if base is None:
            raise ValueError(f"{self.base} is not a concrete base request")
        if self.policy == "value":
            return _facts({"resolved_interface", "resolved_interface_value"})
        if self.policy in {"ref_ptr", "shared_interface", "runtime_ref"}:
            provides = {"resolved_interface"}
            requires = {"shared_storage"}
            modes = ALL_MODES
            if self.policy == "shared_interface":
                provides.add("resolved_shared_interface")
            if self.policy == "runtime_ref":
                requires.add("runtime_reference_conversion_container")
                modes = frozenset({"runtime"})
            return _facts(provides, requires, modes=modes)
        if self.policy == "unique_wrapper":
            return _facts({"resolved_unique_wrapper"}, {"unique_storage"})
        if self.policy == "wrapper":
            dependency = next(iter(base.dependency_forms))
            if dependency == "shared_pointer":
                return _facts(
                    {"resolved_wrapper"},
                    {"shared_storage", "stored_shared_ptr"},
                )
            return _facts(
                {"resolved_wrapper"},
                {"unique_storage", "stored_unique_ptr"},
            )
        raise ValueError(f"unknown interface policy {self.policy}")


INTERFACE_REQUESTS = (
    InterfaceRequest("interface_value", "value", ("copyable_interface_type",), "value"),
    InterfaceRequest("interface_ref_ptr_shared_ptr", "shared_ptr_value", ("interface_type",), "shared_interface"),
    InterfaceRequest("interface_ref", "lvalue_reference_value", ("interface_type", "multiple_interfaces"), "ref_ptr"),
    InterfaceRequest("interface_ptr", "pointer_value", ("interface_type", "multiple_interfaces"), "ref_ptr"),
    InterfaceRequest("second_interface_ref", "lvalue_reference_value", ("multiple_interfaces",), "ref_ptr"),
    InterfaceRequest("multiple_interface_shared_ptr_ref", "lvalue_reference_shared_ptr_value", ("multiple_interfaces",), "runtime_ref"),
    InterfaceRequest("unique_interface_unique_ptr", "unique_ptr_value", ("unique_interface_type",), "unique_wrapper"),
    InterfaceRequest("interface_unique_ptr", "unique_ptr_value", ("unique_interface_type",), "wrapper"),
    InterfaceRequest("interface_shared_ptr", "shared_ptr_value", ("interface_type",), "wrapper"),
)


def concrete_requests() -> tuple[tuple[str, RequestFacts], ...]:
    return tuple((request.name, request.facts()) for request in CONCRETE_REQUESTS)


def qualified_requests() -> tuple[tuple[str, RequestFacts], ...]:
    return tuple(
        (request.name, request.facts())
        for request in (*ANNOTATED_REQUESTS, *KEYED_REQUESTS)
    )


def interface_requests() -> tuple[tuple[str, RequestFacts], ...]:
    return tuple((request.name, request.facts()) for request in INTERFACE_REQUESTS)
