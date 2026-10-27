#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Describe request-shape wrappers by semantics, not by concrete type name.

A shape is a leaf wrapped in zero or more wrappers. The wrapper describes what
it can do -- own, share, borrow, hold nothing, hold an array, hold a variant --
and the shape derives its C++ spelling, flags, and route-rule input from that
description. Concrete standard types are one declaration each, and a user
wrapper is another declaration with the same fields.
"""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Iterable

from tools.model.grammar import cpp
from tools.model.grammar.expr import constructors as constructor_specs


@dataclass(frozen=True)
class WrapperSemantics:
    name: str
    constructor: Callable[[cpp.CppType], cpp.CppType]
    kind: str
    properties: frozenset[str] = frozenset()
    is_reference: bool = False
    is_lvalue_reference: bool = False
    is_pointer: bool = False
    is_array: bool = False
    is_const: bool = False

    def applicable(self, inner: "Shape") -> bool:
        if self.is_const:
            # const applies to the object, not to pointers or references; a
            # pointer-to-const comes from pointer(const(T)).
            return not inner.is_reference and not inner.is_const
        if self.is_reference or self.is_pointer or self.is_array:
            return not inner.is_reference and not inner.is_array
        if self.kind in {"value", "handle", "array_handle", "variant"}:
            return not inner.is_reference and not inner.is_array
        return False

    @property
    def is_access(self) -> bool:
        return self.kind in {
            "reference",
            "pointer",
            "array_reference",
            "array_pointer",
        }

    def has(self, property_name: str) -> bool:
        return property_name in self.properties


def _wrapper(
    name: str,
    constructor: Callable[[cpp.CppType], cpp.CppType],
    kind: str,
    properties: Iterable[str] = (),
    *,
    is_reference: bool = False,
    is_lvalue_reference: bool = False,
    is_pointer: bool = False,
    is_array: bool = False,
    is_const: bool = False,
) -> WrapperSemantics:
    return WrapperSemantics(
        name=name,
        constructor=constructor,
        kind=kind,
        properties=frozenset(properties),
        is_reference=is_reference,
        is_lvalue_reference=is_lvalue_reference,
        is_pointer=is_pointer,
        is_array=is_array,
        is_const=is_const,
    )


WRAPPERS = (
    _wrapper("const", cpp.const, "const", {"const"}, is_const=True),
    _wrapper(
        "lvalue_reference",
        cpp.reference,
        "reference",
        {"borrowable"},
        is_reference=True,
        is_lvalue_reference=True,
    ),
    _wrapper(
        "rvalue_reference",
        cpp.rvalue_reference,
        "reference",
        {"move"},
        is_reference=True,
    ),
    _wrapper(
        "pointer",
        cpp.pointer,
        "pointer",
        {"nullable", "borrowable"},
        is_pointer=True,
    ),
    _wrapper(
        "optional",
        lambda inner: cpp.template("std::optional", inner),
        "value",
        {"owning", "nullable", "borrowable", "in_place", "mapped"},
    ),
    _wrapper(
        "unique_ptr",
        lambda inner: cpp.template("std::unique_ptr", inner),
        "handle",
        {"owning", "sole_owner", "nullable", "borrowable"},
    ),
    _wrapper(
        "shared_ptr",
        lambda inner: cpp.template("std::shared_ptr", inner),
        "handle",
        {"owning", "sharing", "nullable", "borrowable"},
    ),
    _wrapper(
        "unique_array",
        cpp.unique_array,
        "array_handle",
        {"owning", "sole_owner", "nullable", "array_handle"},
    ),
    _wrapper(
        "shared_array",
        cpp.shared_array,
        "array_handle",
        {"owning", "sharing", "nullable", "array_handle"},
    ),
    _wrapper(
        "array_lvalue_reference",
        lambda inner: cpp.array_reference(inner, 2),
        "array_reference",
        {"borrowable"},
        is_reference=True,
        is_lvalue_reference=True,
        is_array=True,
    ),
    _wrapper(
        "array_pointer",
        lambda inner: cpp.array_pointer(inner, 2),
        "array_pointer",
        {"borrowable"},
        is_pointer=True,
        is_array=True,
    ),
    _wrapper(
        "variant",
        lambda inner: cpp.template("std::variant", inner, cpp.leaf("long")),
        "variant",
        {"owning", "in_place", "mapped", "variant"},
    ),
)

WRAPPER_BY_NAME = {wrapper.name: wrapper for wrapper in WRAPPERS}

# Test-only wrapper families. They use the same semantic fields as the built-in
# wrappers so the generated property probe can compare the C++ trait flags with
# one Python declaration.
CUSTOM_WRAPPERS = (
    _wrapper(
        "test_optional",
        lambda inner: cpp.template("dingo::test_optional", inner),
        "value",
        {"owning", "nullable", "borrowable", "in_place"},
    ),
    _wrapper(
        "test_unique",
        lambda inner: cpp.template("dingo::test_unique", inner),
        "handle",
        {"owning", "sole_owner", "nullable", "borrowable"},
    ),
    _wrapper(
        "test_shared",
        lambda inner: cpp.template("dingo::test_shared", inner),
        "handle",
        {"owning", "sharing", "nullable", "borrowable"},
    ),
    _wrapper(
        "test_variant",
        lambda inner: cpp.template("dingo::test_variant", inner),
        "variant",
        {"owning", "in_place", "mapped", "variant"},
    ),
)

@dataclass(frozen=True)
class Shape:
    name: str
    cpp: cpp.CppType
    depth: int = 0
    # Outermost wrapper first.
    wrappers: tuple[WrapperSemantics, ...] = ()
    inner: "Shape | None" = None
    is_reference: bool = False
    is_lvalue_reference: bool = False
    is_pointer: bool = False
    is_array: bool = False
    is_const: bool = False

    @property
    def cpp_type(self) -> str:
        return self.cpp.render()

    @property
    def constructors(self) -> tuple[str, ...]:
        """The wrapper names, outermost first, for compatibility with reports."""
        return tuple(wrapper.name for wrapper in self.wrappers)

    @property
    def outer(self) -> WrapperSemantics | None:
        return self.wrappers[0] if self.wrappers else None

    @property
    def access(self) -> str:
        outer = self.outer
        return outer.name if outer is not None and outer.is_access else "value"

    @property
    def properties(self) -> frozenset[str]:
        return frozenset(
            property_name
            for wrapper in self.wrappers
            for property_name in wrapper.properties
        )

    def has(self, property_name: str) -> bool:
        return property_name in self.properties

    def apply(self, wrapper: WrapperSemantics) -> "Shape":
        if not wrapper.applicable(self):
            raise ValueError(f"{wrapper.name} is not applicable to {self.name}")
        return Shape(
            name=f"{wrapper.name}_{self.name}",
            cpp=wrapper.constructor(self.cpp),
            depth=self.depth + 1,
            wrappers=(wrapper, *self.wrappers),
            inner=self,
            is_reference=wrapper.is_reference,
            is_lvalue_reference=wrapper.is_lvalue_reference,
            is_pointer=wrapper.is_pointer,
            is_array=wrapper.is_array,
            is_const=wrapper.is_const,
        )

    def with_leaf(self, leaf: str) -> str:
        """Render the shape with a different leaf type spelling."""
        return cpp.rebase_leaf(self.cpp, leaf).render()


BASE = Shape("value", cpp.leaf("int"))
SHAPE_DEPTH = 2


def enumerate_shapes(depth: int = SHAPE_DEPTH) -> tuple[Shape, ...]:
    shapes = {BASE.name: BASE}
    frontier = [BASE]
    for _ in range(depth):
        next_frontier = []
        for inner in frontier:
            for wrapper in WRAPPERS:
                if not wrapper.applicable(inner):
                    continue
                shape = inner.apply(wrapper)
                if shape.name in shapes:
                    continue
                shapes[shape.name] = shape
                next_frontier.append(shape)
        frontier = next_frontier
    # A bare array or a top-level const is not a request type. Reference and
    # pointer forms of arrays are included.
    return tuple(
        sorted(
            (
                shape
                for shape in shapes.values()
                if shape.depth == 0 or not shape.is_const
            ),
            key=lambda shape: shape.name,
        )
    )


# Properties that identify the shape a wrapper represents. Construction
# details such as in_place/mapped do not distinguish a custom family from the
# built-in wrapper it mirrors.
SHAPE_PROPERTIES = frozenset(
    {
        "owning",
        "sharing",
        "sole_owner",
        "nullable",
        "borrowable",
        "array_handle",
        "variant",
    }
)


def wrapper_signature(wrapper: WrapperSemantics) -> tuple[str, frozenset[str]]:
    return (
        wrapper.kind,
        frozenset(property_name for property_name in wrapper.properties if property_name in SHAPE_PROPERTIES),
    )


def partner_for(custom: WrapperSemantics) -> WrapperSemantics | None:
    """Find the built-in wrapper with the same shape signature."""
    signature = wrapper_signature(custom)
    candidates = [
        wrapper for wrapper in WRAPPERS if wrapper_signature(wrapper) == signature
    ]
    if len(candidates) > 1:
        names = ", ".join(wrapper.name for wrapper in candidates)
        raise ValueError(f"ambiguous built-in partner for {custom.name}: {names}")
    return candidates[0] if candidates else None


def enumerate_custom_shapes() -> tuple[Shape, ...]:
    shapes = {}
    for custom in CUSTOM_WRAPPERS:
        own = BASE.apply(custom)
        shapes[own.name] = own
        partner = partner_for(custom)
        if partner is None:
            continue
        inner = BASE.apply(partner)
        if custom.applicable(inner):
            shape = inner.apply(custom)
            shapes[shape.name] = shape
        if partner.applicable(own):
            shape = own.apply(partner)
            shapes[shape.name] = shape
    return tuple(sorted(shapes.values(), key=lambda shape: shape.name))


CUSTOM_SHAPES = enumerate_custom_shapes()
SHAPES = tuple(
    sorted((*enumerate_shapes(), *CUSTOM_SHAPES), key=lambda shape: shape.name)
)

# The constructor registry is the source of op names; every wrapper-like op
# must have semantics here. Adding a wrapper to one side without the other is a
# startup error rather than a silently missing shape.
_WRAPPER_ALIASES = {"array_reference": "array_lvalue_reference"}
_SEMANTIC_NAMES = {wrapper.name for wrapper in WRAPPERS}
for _spec in (
    *constructor_specs("qualifier"),
    *constructor_specs("wrapper"),
    *constructor_specs("variant"),
):
    _name = _WRAPPER_ALIASES.get(_spec.name, _spec.name)
    if _name not in _SEMANTIC_NAMES:
        raise ValueError(
            f"constructor {_spec.name!r} has no wrapper semantics"
        )
