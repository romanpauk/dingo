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

from spec.model.grammar import cpp
from spec.model.grammar.expr import Expr


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
        if self.kind == "annotated_interface":
            return f"annotated_interface:{self.tag}:{','.join(self.interfaces)}"
        return self.kind

    def request_type(self, request: Expr) -> cpp.CppType:
        """The concrete C++ type the caller resolves through this binding."""
        if self.kind == "interface":
            return cpp.with_leaf(request.inner.cpp(), self.interfaces[0])
        if self.kind == "annotated_interface":
            target = cpp.with_leaf(
                request.inner.inner.cpp(), self.interfaces[0]
            )
            return cpp.annotated(target, self.tag)
        return request.cpp()

    def registration_interfaces(
        self, leaf: cpp.CppType
    ) -> tuple[cpp.CppType, ...]:
        if self.kind == "annotated":
            return (cpp.annotated(leaf, self.tag),)
        if self.kind == "interface":
            return (cpp.leaf(self.interfaces[0]),)
        if self.kind == "annotated_interface":
            return (
                cpp.annotated(cpp.leaf(self.interfaces[0]), self.tag),
            )
        return ()

    def registration_key(self) -> cpp.CppType | None:
        if self.kind == "keyed":
            return cpp.template("dingo::key_type", cpp.text(self.key))
        return None

    def exposes(self, request: Expr) -> bool:
        if request.op == "annotated":
            if request.inner.op == "interface":
                return (
                    self.kind == "annotated_interface"
                    and self.tag == request.name
                    and request.inner.name in self.interfaces
                )
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
ANNOTATED_INTERFACE = Binding(
    "annotated_interface",
    tag="tag_a",
    interfaces=("interface_type",),
)


def binding_for(request: Expr) -> Binding:
    if request.op == "annotated":
        if request.inner.op == "interface":
            return Binding(
                "annotated_interface",
                tag=request.name,
                interfaces=(request.inner.name,),
            )
        return Binding("annotated", tag=request.name)
    if request.op == "keyed":
        return Binding("keyed", key=request.name)
    if request.op == "interface":
        return Binding("interface", interfaces=(request.name,))
    return CONCRETE
