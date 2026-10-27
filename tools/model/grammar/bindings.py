#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The binding/selection side of a type-path case.

A request is not enough to decide support: the binding decides what the
registration exposes. The first slice covers the concrete and annotated
bindings; interface, keyed, and collection bindings follow the same shape.
"""

from __future__ import annotations

from dataclasses import dataclass

from tools.model.grammar import cpp
from tools.model.grammar.expr import Expr


@dataclass(frozen=True, slots=True)
class Binding:
    kind: str
    tag: str = ""
    key: str = ""
    interfaces: tuple[str, ...] = ()
    copyable: bool = False

    @property
    def label(self) -> str:
        if self.kind == "annotated":
            return f"annotated:{self.tag}"
        return self.kind

    def request_type(self, request: Expr) -> cpp.CppType:
        """The concrete C++ type the caller resolves through this binding."""
        if self.kind == "interface":
            return cpp.with_leaf(request.inner.cpp(), self.interfaces[0])
        return request.cpp()

    def registration_interfaces(
        self, leaf: cpp.CppType
    ) -> tuple[cpp.CppType, ...]:
        if self.kind == "annotated":
            return (cpp.annotated(leaf, self.tag),)
        if self.kind == "interface":
            return (cpp.leaf(self.interfaces[0]),)
        return ()

    def registration_key(self) -> cpp.CppType | None:
        if self.kind == "keyed":
            return cpp.template("dingo::key_type", cpp.text(self.key))
        return None

    def exposes(self, request: Expr) -> bool:
        if request.op == "annotated":
            return self.kind == "annotated" and self.tag == request.name
        if request.op == "keyed":
            return self.kind == "keyed" and self.key == request.name
        if request.op == "interface":
            return self.kind == "interface" and request.name in self.interfaces
        return self.kind in {"concrete", "interface"}


CONCRETE = Binding("concrete")
ANNOTATED = Binding("annotated", tag="tag_a")
KEYED = Binding("keyed", key="key_a")
INTERFACE = Binding("interface", interfaces=("interface_type",))


def binding_for(request: Expr) -> Binding:
    if request.op == "annotated":
        return Binding("annotated", tag=request.name)
    if request.op == "keyed":
        return Binding("keyed", key=request.name)
    if request.op == "interface":
        return Binding("interface", interfaces=(request.name,))
    return CONCRETE
