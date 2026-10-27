#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Storage paths generated from the type grammar.

A storage path is an expression that can actually be stored by a scope. Const
is a normal grammar constructor here; there is no separate const table.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

from tools.model.grammar.catalog import FORM_BY_NAME, SCOPES, Scope, StorageForm
from tools.model.grammar import cpp
from tools.model.grammar.enumerate import storage_types
from tools.model.grammar.expr import (
    Expr,
    array,
    leaf,
    optional,
    pointer,
    shared_array,
    shared_ptr,
    unique_array,
    unique_ptr,
    variant,
)


@dataclass(frozen=True)
class StoragePath:
    scope: Scope
    expr: Expr
    form_name: str
    name: str

    @property
    def form(self) -> StorageForm:
        return FORM_BY_NAME[self.form_name]

    def cpp(self, leaf: str = "tracked") -> cpp.CppType:
        """The stored C++ type, with the placeholder leaf replaced."""
        return self.expr.with_leaf(leaf).cpp()

    def cpp_type(self, leaf: str = "tracked") -> str:
        return self.cpp(leaf).render()

    def storage_type(self, leaf: str = "tracked") -> cpp.CppType:
        return cpp.storage(self.cpp(leaf))

    def storage(self, leaf: str = "tracked") -> str:
        return self.storage_type(leaf).render()

    def factory(self, leaf: str = "tracked") -> str | None:
        return self.form.factory(leaf)

    def const_factory(self, leaf: str = "tracked") -> str | None:
        return self.form.const_factory(leaf)


def storable(scope: Scope, expr: Expr) -> StorageForm | None:
    """The storage form for an expression in a scope, or None."""
    if expr.op in {
        "array_reference",
        "array_pointer",
        "rvalue_reference",
        "annotated",
        "keyed",
        "interface",
    }:
        return None
    if expr.op == "lvalue_reference":
        if scope.name != "external":
            return None
        return FORM_BY_NAME["plain"]
    if expr.op == "const":
        return storable(scope, expr.inner)
    if expr.op == "leaf":
        return FORM_BY_NAME["plain"]
    if expr.op == "pointer":
        return FORM_BY_NAME["pointer"]
    if expr.op == "array":
        return FORM_BY_NAME["bounded_array"]
    if expr.op == "unique_ptr":
        return FORM_BY_NAME["unique_handle"]
    if expr.op == "unique_array":
        return FORM_BY_NAME["unique_array_handle"]
    if expr.op == "shared_ptr":
        return FORM_BY_NAME["shared_handle"]
    if expr.op == "shared_array":
        return FORM_BY_NAME["shared_array_handle"]
    if expr.op == "optional":
        return FORM_BY_NAME["optional"]
    if expr.op == "variant":
        return FORM_BY_NAME["alternative"]
    return None


_OLD_NAMES = {
    leaf("T"): "plain",
    pointer(leaf("T")): "pointer",
    array(leaf("T"), 2): "bounded_array_2",
    array(leaf("T"), 3): "bounded_array_3",
    array(array(leaf("T"), 2), 3): "bounded_array_2_3",
    unique_ptr(leaf("T")): "unique_handle",
    unique_array(leaf("T")): "unique_array_handle",
    shared_ptr(leaf("T")): "shared_handle",
    shared_array(leaf("T")): "shared_array_handle",
    optional(leaf("T")): "optional",
    variant(leaf("T"), leaf("long")): "alternative",
}


def _slug(expr: Expr) -> str:
    text = re.sub(r"[^a-z0-9]+", "_", expr.render().lower()).strip("_")
    return text


def _name(expr: Expr, form: StorageForm) -> str:
    return _OLD_NAMES.get(expr, f"{form.name}_{expr.op}_{_slug(expr)}")


def storage_paths() -> tuple[StoragePath, ...]:
    paths = []
    seen = set()
    for scope in SCOPES:
        for expr in storage_types():
            form = storable(scope, expr)
            if form is None:
                continue
            name = _name(expr, form)
            if (scope.name, name) in seen:
                continue
            seen.add((scope.name, name))
            paths.append(StoragePath(scope, expr, form.name, name))
    return tuple(sorted(paths, key=lambda path: (path.scope.name, path.name)))


STORAGE_PATHS = storage_paths()
STORAGE_BY_KEY = {(path.scope.name, path.name): path for path in STORAGE_PATHS}
FORM_BY_STORAGE_NAME = {
    path.name: path.form_name for path in STORAGE_PATHS
}


def storage_path(scope: str, name: str) -> StoragePath | None:
    return STORAGE_BY_KEY.get((scope, name))


def has_const(expr: Expr) -> bool:
    if expr.op == "const":
        return True
    if expr.inner is not None and has_const(expr.inner):
        return True
    return any(has_const(alternative) for alternative in expr.alternatives)


def const_leaf(expr: Expr) -> bool:
    """Whether the stored object/pointee is const-qualified."""
    if expr.op == "const":
        inner = expr.inner
        if inner.op == "pointer":
            return const_leaf(inner)
        return inner.op in {"leaf", "array"}
    if expr.op == "pointer":
        return const_leaf(expr.inner)
    if expr.op in {
        "array",
        "optional",
        "unique_ptr",
        "shared_ptr",
        "unique_array",
        "shared_array",
    }:
        return const_leaf(expr.inner)
    if expr.op == "variant":
        return all(const_leaf(item) for item in expr.alternatives)
    return False
