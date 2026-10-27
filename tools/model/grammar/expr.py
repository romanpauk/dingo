#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The type-expression grammar shared by the coverage paths.

A type is a leaf and a path of constructors: `const`, references, pointers,
arrays, wrappers, variants, and annotations. The grammar is canonical -- the
constructors reject combinations C++ itself rejects -- and `render()` gives the
C++ spelling used by the generated probes.
"""

from __future__ import annotations

from dataclasses import dataclass

LEAF_NAMES = ("leaf",)
QUALIFIER_NAMES = (
    "const",
    "lvalue_reference",
    "rvalue_reference",
    "pointer",
)
REFERENCE_NAMES = ("lvalue_reference", "rvalue_reference")
ARRAY_NAMES = ("array", "array_reference", "array_pointer")
WRAPPER_NAMES = (
    "optional",
    "unique_ptr",
    "shared_ptr",
    "unique_array",
    "shared_array",
)
VARIANT_NAMES = ("variant",)
BINDING_NAMES = ("annotated", "keyed", "interface")

WRAPPERS = WRAPPER_NAMES
WRAPPER_OPS = frozenset(WRAPPER_NAMES)
REFERENCE_OPS = frozenset(REFERENCE_NAMES)
ARRAY_OPS = frozenset(ARRAY_NAMES)
BINDING_OPS = frozenset(BINDING_NAMES)
OBJECT_OPS = frozenset(
    {"leaf", "const", "pointer", "optional", "unique_ptr", "shared_ptr", "variant"}
)


@dataclass(frozen=True, slots=True)
class Expr:
    op: str
    name: str = ""
    inner: "Expr | None" = None
    extent: int | None = None
    alternatives: tuple["Expr", ...] = ()

    def cpp(self):
        """The structured C++ type for this expression."""
        from tools.model.grammar import cpp

        if self.op == "leaf":
            return cpp.leaf(self.name)
        if self.op == "const":
            return cpp.const(self.inner.cpp())
        if self.op == "lvalue_reference":
            return cpp.reference(self.inner.cpp())
        if self.op == "rvalue_reference":
            return cpp.rvalue_reference(self.inner.cpp())
        if self.op == "pointer":
            return cpp.pointer(self.inner.cpp())
        if self.op == "array":
            return cpp.array(self.inner.cpp(), self.extent)
        if self.op == "array_reference":
            return cpp.array_reference(self.inner.cpp(), self.extent)
        if self.op == "array_pointer":
            return cpp.array_pointer(self.inner.cpp(), self.extent)
        if self.op == "optional":
            return cpp.template("std::optional", self.inner.cpp())
        if self.op == "unique_ptr":
            return cpp.template("std::unique_ptr", self.inner.cpp())
        if self.op == "shared_ptr":
            return cpp.template("std::shared_ptr", self.inner.cpp())
        if self.op == "unique_array":
            return cpp.unique_array(self.inner.cpp())
        if self.op == "shared_array":
            return cpp.shared_array(self.inner.cpp())
        if self.op == "variant":
            return cpp.template(
                "std::variant",
                *(item.cpp() for item in self.alternatives),
            )
        if self.op == "annotated":
            return cpp.annotated(self.inner.cpp(), self.name)
        if self.op == "keyed":
            return self.inner.cpp()
        if self.op == "interface":
            return cpp.template("interface", self.inner.cpp(), cpp.text(self.name))
        raise ValueError(f"unknown expression op {self.op!r}")

    def render(self) -> str:
        return self.cpp().render()

    def with_leaf(self, name: str) -> "Expr":
        if self.op == "leaf":
            if self.name != "T":
                return self
            return Expr("leaf", name=name)
        if self.op == "variant":
            return Expr(
                "variant",
                alternatives=tuple(item.with_leaf(name) for item in self.alternatives),
            )
        return Expr(
            self.op,
            name=self.name,
            inner=self.inner.with_leaf(name) if self.inner is not None else None,
            extent=self.extent,
        )


def leaf(name: str = "T") -> Expr:
    return Expr("leaf", name=name)


def _qualify(op: str, inner: Expr) -> Expr | None:
    if op == "const":
        if inner.op in REFERENCE_OPS | ARRAY_OPS | {"const"}:
            return None
        return Expr(op, inner=inner)
    if op in REFERENCE_OPS:
        if inner.op in REFERENCE_OPS | ARRAY_OPS:
            return None
        return Expr(op, inner=inner)
    if op == "pointer":
        if inner.op in REFERENCE_OPS | ARRAY_OPS:
            return None
        return Expr(op, inner=inner)
    raise ValueError(f"unknown qualifier {op!r}")


def const(inner: Expr) -> Expr | None:
    return _qualify("const", inner)


def lvalue_reference(inner: Expr) -> Expr | None:
    return _qualify("lvalue_reference", inner)


def rvalue_reference(inner: Expr) -> Expr | None:
    return _qualify("rvalue_reference", inner)


def pointer(inner: Expr) -> Expr | None:
    return _qualify("pointer", inner)


def array(inner: Expr, extent: int) -> Expr | None:
    if inner.op in REFERENCE_OPS:
        return None
    return Expr("array", inner=inner, extent=extent)


def array_reference(inner: Expr, extent: int) -> Expr | None:
    if inner.op in REFERENCE_OPS | ARRAY_OPS:
        return None
    return Expr("array_reference", inner=inner, extent=extent)


def array_pointer(inner: Expr, extent: int) -> Expr | None:
    if inner.op in REFERENCE_OPS | ARRAY_OPS:
        return None
    return Expr("array_pointer", inner=inner, extent=extent)


def _wrapper(op: str, inner: Expr) -> Expr | None:
    if inner.op in REFERENCE_OPS | ARRAY_OPS:
        return None
    return Expr(op, inner=inner)


def optional(inner: Expr) -> Expr | None:
    return _wrapper("optional", inner)


def unique_ptr(inner: Expr) -> Expr | None:
    return _wrapper("unique_ptr", inner)


def shared_ptr(inner: Expr) -> Expr | None:
    return _wrapper("shared_ptr", inner)


def unique_array(inner: Expr) -> Expr | None:
    if inner.op in REFERENCE_OPS | ARRAY_OPS or inner.op == "array":
        return None
    return Expr("unique_array", inner=inner)


def shared_array(inner: Expr) -> Expr | None:
    if inner.op in REFERENCE_OPS | ARRAY_OPS or inner.op == "array":
        return None
    return Expr("shared_array", inner=inner)


def variant(*alternatives: Expr) -> Expr | None:
    if len(alternatives) < 2:
        return None
    if any(item.op in REFERENCE_OPS | ARRAY_OPS for item in alternatives):
        return None
    return Expr("variant", alternatives=tuple(alternatives))


def annotated(inner: Expr, tag: str = "tag_a") -> Expr:
    return Expr("annotated", name=tag, inner=inner)


def keyed(inner: Expr, key: str = "key_a") -> Expr:
    return Expr("keyed", name=key, inner=inner)


def interface(inner: Expr, name: str = "interface_type") -> Expr:
    return Expr("interface", name=name, inner=inner)


def valid(expr: Expr) -> bool:
    spec = CONSTRUCTOR_BY_NAME.get(expr.op)
    if spec is None:
        return False
    if spec.kind == "leaf":
        return bool(expr.name)
    if spec.kind == "variant":
        return len(expr.alternatives) >= 2 and all(
            valid(item)
            and item.op not in REFERENCE_OPS | ARRAY_OPS
            for item in expr.alternatives
        )
    if expr.inner is None or not valid(expr.inner):
        return False
    if expr.op in ARRAY_OPS:
        return expr.extent is not None and expr.extent > 0
    return True


def family(expr: Expr) -> str:
    """The outermost constructor, with leaves reported as `value`."""
    if expr.op == "leaf":
        return "value"
    return expr.op


@dataclass(frozen=True, slots=True)
class Constructor:
    name: str
    kind: str
    make: object
    indirection: bool = False


def _constructor(name: str, kind: str, indirection: bool = False) -> Constructor:
    return Constructor(name, kind, globals()[name], indirection)


CONSTRUCTORS = (
    *(_constructor(name, "leaf") for name in LEAF_NAMES),
    *(
        _constructor(name, "qualifier", name == "pointer")
        for name in QUALIFIER_NAMES
    ),
    *(_constructor(name, "array") for name in ARRAY_NAMES),
    *(
        _constructor(
            name,
            "wrapper",
            name in {"unique_ptr", "shared_ptr", "unique_array", "shared_array"},
        )
        for name in WRAPPER_NAMES
    ),
    *(_constructor(name, "variant") for name in VARIANT_NAMES),
    *(_constructor(name, "binding") for name in BINDING_NAMES),
)
CONSTRUCTOR_BY_NAME = {spec.name: spec for spec in CONSTRUCTORS}
if len(CONSTRUCTOR_BY_NAME) != len(CONSTRUCTORS):
    raise ValueError("duplicate constructor name")
INDIRECTION_OPS = frozenset(
    spec.name for spec in CONSTRUCTORS if spec.indirection
)


def constructors(kind: str) -> tuple[Constructor, ...]:
    return tuple(spec for spec in CONSTRUCTORS if spec.kind == kind)


def constructor_names(kind: str | None = None) -> tuple[str, ...]:
    return tuple(
        spec.name
        for spec in CONSTRUCTORS
        if kind is None or spec.kind == kind
    )
