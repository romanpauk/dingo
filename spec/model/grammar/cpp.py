#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""A small C++ type algebra.

All C++ spelling is rendered here. The rest of the model builds `CppType`
values and never concatenates type syntax or parses a rendered type back into
its parts.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class CppKind(str, Enum):
    LEAF = "leaf"
    TEXT = "text"
    CONST = "const"
    POINTER = "pointer"
    REFERENCE = "reference"
    RVALUE_REFERENCE = "rvalue_reference"
    ARRAY = "array"
    RAW_ARRAY = "raw_array"
    ARRAY_REFERENCE = "array_reference"
    ARRAY_POINTER = "array_pointer"
    UNIQUE_ARRAY = "unique_array"
    SHARED_ARRAY = "shared_array"
    TEMPLATE = "template"


@dataclass(frozen=True)
class CppType:
    kind: CppKind
    name: str = ""
    inner: "CppType | None" = None
    args: tuple["CppType", ...] = ()
    extent: int | None = None

    def render(self) -> str:
        return render(self)


def leaf(name: str) -> CppType:
    return CppType(CppKind.LEAF, name=name)


def text(value: str) -> CppType:
    return CppType(CppKind.TEXT, name=value)


def const(inner: CppType) -> CppType:
    return CppType(CppKind.CONST, inner=inner)


def pointer(inner: CppType) -> CppType:
    return CppType(CppKind.POINTER, inner=inner)


def reference(inner: CppType) -> CppType:
    return CppType(CppKind.REFERENCE, inner=inner)


def rvalue_reference(inner: CppType) -> CppType:
    return CppType(CppKind.RVALUE_REFERENCE, inner=inner)


def array(inner: CppType, extent: int) -> CppType:
    return CppType(CppKind.ARRAY, inner=inner, extent=extent)


def raw_array(inner: CppType) -> CppType:
    return CppType(CppKind.RAW_ARRAY, inner=inner)


def array_reference(inner: CppType, extent: int) -> CppType:
    return CppType(CppKind.ARRAY_REFERENCE, inner=inner, extent=extent)


def array_pointer(inner: CppType, extent: int) -> CppType:
    return CppType(CppKind.ARRAY_POINTER, inner=inner, extent=extent)


def unique_array(inner: CppType) -> CppType:
    return CppType(CppKind.UNIQUE_ARRAY, inner=inner)


def shared_array(inner: CppType) -> CppType:
    return CppType(CppKind.SHARED_ARRAY, inner=inner)


def template(name: str, *args: CppType) -> CppType:
    return CppType(CppKind.TEMPLATE, name=name, args=tuple(args))


def storage(inner: CppType) -> CppType:
    return template("dingo::storage", inner)


def annotated(inner: CppType, tag: str) -> CppType:
    return template("dingo::annotated", inner, text(tag))


def interfaces(*types: CppType) -> CppType:
    return template("dingo::interfaces", *types)


def with_leaf(value: CppType, name: str, placeholder: str = "T") -> CppType:
    """Replace the placeholder leaf, preserving fixed alternatives."""
    kind = value.kind
    if kind is CppKind.LEAF:
        return leaf(name if value.name == placeholder else value.name)
    if kind is CppKind.TEXT:
        return value
    if kind is CppKind.CONST:
        return const(with_leaf(value.inner, name, placeholder))
    if kind is CppKind.POINTER:
        return pointer(with_leaf(value.inner, name, placeholder))
    if kind is CppKind.REFERENCE:
        return reference(with_leaf(value.inner, name, placeholder))
    if kind is CppKind.RVALUE_REFERENCE:
        return rvalue_reference(with_leaf(value.inner, name, placeholder))
    if kind is CppKind.ARRAY:
        return array(with_leaf(value.inner, name, placeholder), value.extent)
    if kind is CppKind.RAW_ARRAY:
        return raw_array(with_leaf(value.inner, name, placeholder))
    if kind is CppKind.ARRAY_REFERENCE:
        return array_reference(
            with_leaf(value.inner, name, placeholder), value.extent
        )
    if kind is CppKind.ARRAY_POINTER:
        return array_pointer(
            with_leaf(value.inner, name, placeholder), value.extent
        )
    if kind is CppKind.UNIQUE_ARRAY:
        return unique_array(with_leaf(value.inner, name, placeholder))
    if kind is CppKind.SHARED_ARRAY:
        return shared_array(with_leaf(value.inner, name, placeholder))
    if kind is CppKind.TEMPLATE:
        return template(
            value.name,
            *(with_leaf(item, name, placeholder) for item in value.args),
        )
    raise ValueError(f"unknown C++ type kind {kind!r}")


def rebase_leaf(value: CppType, name: str) -> CppType:
    """Replace the concrete base leaf (``int``) of a shape spelling."""
    return with_leaf(value, name, placeholder="int")


def declaration(value: CppType, name: str) -> str:
    """Render a variable declaration preserving array declarators."""
    kind = value.kind
    if kind is CppKind.ARRAY:
        return declaration(value.inner, name) + f"[{value.extent}]"
    if kind is CppKind.RAW_ARRAY:
        return declaration(value.inner, name) + "[]"
    return f"{render(value)} {name}"


def render(value: CppType) -> str:
    kind = value.kind
    if kind is CppKind.LEAF:
        return value.name
    if kind is CppKind.TEXT:
        return value.name
    if kind is CppKind.CONST:
        rendered = render(value.inner)
        if value.inner.kind is CppKind.POINTER:
            return f"{rendered} const"
        return f"const {rendered}"
    if kind is CppKind.POINTER:
        return f"{render(value.inner)} *"
    if kind is CppKind.REFERENCE:
        return f"{render(value.inner)} &"
    if kind is CppKind.RVALUE_REFERENCE:
        return f"{render(value.inner)} &&"
    if kind is CppKind.ARRAY:
        return f"{render(value.inner)}[{value.extent}]"
    if kind is CppKind.RAW_ARRAY:
        return f"{render(value.inner)}[]"
    if kind is CppKind.ARRAY_REFERENCE:
        return f"{render(value.inner)} (&)[{value.extent}]"
    if kind is CppKind.ARRAY_POINTER:
        return f"{render(value.inner)} (*)[{value.extent}]"
    if kind is CppKind.UNIQUE_ARRAY:
        return f"std::unique_ptr<{render(value.inner)}[]>"
    if kind is CppKind.SHARED_ARRAY:
        return f"std::shared_ptr<{render(value.inner)}[]>"
    if kind is CppKind.TEMPLATE:
        arguments = ", ".join(render(item) for item in value.args)
        return f"{value.name}<{arguments}>"
    raise ValueError(f"unknown C++ type kind {kind!r}")
