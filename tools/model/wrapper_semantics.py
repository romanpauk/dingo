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

from dataclasses import dataclass
from typing import Iterable


@dataclass(frozen=True)
class WrapperSemantics:
    name: str
    render: str
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
    render: str,
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
        render=render,
        kind=kind,
        properties=frozenset(properties),
        is_reference=is_reference,
        is_lvalue_reference=is_lvalue_reference,
        is_pointer=is_pointer,
        is_array=is_array,
        is_const=is_const,
    )


WRAPPERS = (
    _wrapper("const", "const {inner}", "const", {"const"}, is_const=True),
    _wrapper(
        "lvalue_reference",
        "{inner} &",
        "reference",
        {"borrowable"},
        is_reference=True,
        is_lvalue_reference=True,
    ),
    _wrapper(
        "rvalue_reference",
        "{inner} &&",
        "reference",
        {"move"},
        is_reference=True,
    ),
    _wrapper("pointer", "{inner} *", "pointer", {"borrowable"}, is_pointer=True),
    _wrapper(
        "optional",
        "std::optional<{inner}>",
        "value",
        {"owning", "nullable", "borrowable", "in_place", "mapped"},
    ),
    _wrapper(
        "unique_ptr",
        "std::unique_ptr<{inner}>",
        "handle",
        {"owning", "sole_owner", "borrowable"},
    ),
    _wrapper(
        "shared_ptr",
        "std::shared_ptr<{inner}>",
        "handle",
        {"owning", "sharing", "borrowable"},
    ),
    _wrapper(
        "unique_array",
        "std::unique_ptr<{inner}[]>",
        "array_handle",
        {"owning", "sole_owner", "borrowable", "array_handle"},
    ),
    _wrapper(
        "shared_array",
        "std::shared_ptr<{inner}[]>",
        "array_handle",
        {"owning", "sharing", "borrowable", "array_handle"},
    ),
    _wrapper(
        "array_lvalue_reference",
        "{inner} (&)[2]",
        "array_reference",
        {"borrowable"},
        is_reference=True,
        is_lvalue_reference=True,
        is_array=True,
    ),
    _wrapper(
        "array_pointer",
        "{inner} (*)[2]",
        "array_pointer",
        {"borrowable"},
        is_pointer=True,
        is_array=True,
    ),
    _wrapper(
        "variant",
        "std::variant<{inner}, long>",
        "variant",
        {"owning", "in_place", "mapped", "variant"},
    ),
)

WRAPPER_BY_NAME = {wrapper.name: wrapper for wrapper in WRAPPERS}

# The wrapper each storage form stores. `bounded_array` and `array` are array
# storage, not a wrapper shape; `plain` is the bare leaf.
FORM_WRAPPERS = {
    "plain": None,
    "alternative": WRAPPER_BY_NAME["variant"],
    "pointer": WRAPPER_BY_NAME["pointer"],
    "bounded_array": None,
    "unique_handle": WRAPPER_BY_NAME["unique_ptr"],
    "unique_array_handle": WRAPPER_BY_NAME["unique_array"],
    "shared_handle": WRAPPER_BY_NAME["shared_ptr"],
    "shared_array_handle": WRAPPER_BY_NAME["shared_array"],
    "optional": WRAPPER_BY_NAME["optional"],
    "array": None,
}

ARRAY_FORMS = frozenset({"array", "bounded_array"})
ARRAY_HANDLE_FORMS = frozenset({"unique_array_handle", "shared_array_handle"})
HANDLE_FORMS = frozenset(
    {"unique_handle", "shared_handle", "unique_array_handle", "shared_array_handle"}
)


def form_family(form_name: str) -> str:
    if form_name == "plain":
        return "leaf"
    if form_name == "alternative":
        return "variant"
    if form_name == "pointer":
        return "pointer"
    if form_name in ARRAY_FORMS:
        return "array"
    if form_name in ARRAY_HANDLE_FORMS:
        return "array_handle"
    if form_name in HANDLE_FORMS:
        return "handle"
    if form_name == "optional":
        return "optional"
    return "unknown"


def form_wrapper(form_name: str) -> WrapperSemantics | None:
    return FORM_WRAPPERS.get(form_name)


@dataclass(frozen=True)
class Shape:
    name: str
    cpp_type: str
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
            cpp_type=wrapper.render.format(inner=self.cpp_type),
            depth=self.depth + 1,
            wrappers=(wrapper, *self.wrappers),
            inner=self,
            is_reference=wrapper.is_reference,
            is_lvalue_reference=wrapper.is_lvalue_reference,
            is_pointer=wrapper.is_pointer,
            is_array=wrapper.is_array,
            is_const=wrapper.is_const,
        )


BASE = Shape("value", "int")
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


SHAPES = enumerate_shapes()
