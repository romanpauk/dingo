#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The storage catalog shared by every model family.

A scope is a policy over a source. A storage form is a semantic kind plus an
optional wrapper; the C++ spelling, factory, and external-instance recipe are
derived from those two facts and a leaf name. This is the only module that
describes the built-in scope and form set.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum

from tools.model.grammar import cpp
from tools.model.grammar.shapes import WRAPPER_BY_NAME, WrapperSemantics


@dataclass(frozen=True)
class Scope:
    name: str
    cpp_type: str
    header: str
    stable: bool
    consumes: bool
    guard: str
    retains_unresolved_source: bool
    allows_cycles: bool
    returns_pointer: bool
    can_be_static: bool = True


SCOPES = (
    Scope("shared", "dingo::shared", "dingo/storage/shared.h", True, False,
          "while_unresolved", True, False, True),
    Scope("unique", "dingo::unique", "dingo/storage/unique.h", False, True,
          "always", False, False, False),
    Scope("external", "dingo::external", "dingo/storage/external.h", True, False,
          "none", False, False, False, can_be_static=False),
    Scope("shared_cyclical", "dingo::shared_cyclical",
          "dingo/storage/shared_cyclical.h", False, False, "none", False, True,
          True),
)


class FormConstructor(str, Enum):
    VALUE = "value"
    REFERENCE = "reference"
    POINTER = "pointer"
    ARRAY = "array"
    RAW_ARRAY = "raw_array"
    VARIANT = "variant"
    OPTIONAL = "optional"
    UNIQUE_HANDLE = "unique_handle"
    SHARED_HANDLE = "shared_handle"
    UNIQUE_ARRAY = "unique_array"
    SHARED_ARRAY = "shared_array"

    def apply(self, element: cpp.CppType) -> cpp.CppType:
        if self is FormConstructor.VALUE:
            return element
        if self is FormConstructor.REFERENCE:
            return cpp.reference(element)
        if self is FormConstructor.POINTER:
            return cpp.pointer(element)
        if self is FormConstructor.ARRAY:
            return cpp.array(element, 2)
        if self is FormConstructor.RAW_ARRAY:
            return cpp.raw_array(element)
        if self is FormConstructor.VARIANT:
            return cpp.template("std::variant", element, cpp.leaf("long"))
        if self is FormConstructor.OPTIONAL:
            return cpp.template("std::optional", element)
        if self is FormConstructor.UNIQUE_HANDLE:
            return cpp.template("std::unique_ptr", element)
        if self is FormConstructor.SHARED_HANDLE:
            return cpp.template("std::shared_ptr", element)
        if self is FormConstructor.UNIQUE_ARRAY:
            return cpp.unique_array(element)
        if self is FormConstructor.SHARED_ARRAY:
            return cpp.shared_array(element)
        raise ValueError(f"unknown form constructor {self!r}")


@dataclass(frozen=True)
class Factory:
    kind: str
    element: cpp.CppType
    const: bool = False

    def _token(self) -> str:
        element = self.element
        if element.kind is cpp.CppKind.LEAF:
            return element.name
        if element.kind is cpp.CppKind.CONST and element.inner.kind is cpp.CppKind.LEAF:
            return f"const_{element.inner.name}"
        raise ValueError(f"factory element is not a leaf: {element}")

    @property
    def name(self) -> str:
        token = self._token()
        if self.const:
            token = f"const_{token}"
        return f"make_{self.kind}_{token}"


@dataclass(frozen=True)
class StorageForm:
    """One way the library stores a leaf.

    ``constructor`` is the model: it turns an element C++ type into the stored
    type. Factory names and storage spellings are derived from that constructor;
    they are never stored as formatting templates.
    """

    name: str
    kind: str
    wrapper_name: str | None
    constructor: FormConstructor
    headers: tuple[str, ...]
    has_value: bool
    has_lvalue: bool
    has_rvalue_reference: bool
    has_pointer: bool
    sharing: bool = False
    factory_kind: str | None = None
    external_constructor: FormConstructor | None = None
    # A bare form registers a caller-owned instance directly instead of a
    # factory result.
    external_leaf_only: bool = False
    # A form may need a factory only for its external instance while the
    # registration default-constructs the stored type.
    registration_factory: bool = True

    @property
    def wrapper(self) -> WrapperSemantics | None:
        if self.wrapper_name is None:
            return None
        return WRAPPER_BY_NAME[self.wrapper_name]

    def element(self, leaf: str = "int") -> cpp.CppType:
        return cpp.leaf(leaf)

    def cpp(self, leaf: str = "int") -> cpp.CppType:
        return self.constructor.apply(self.element(leaf))

    def cpp_type(self, leaf: str = "int") -> str:
        return self.cpp(leaf).render()

    def storage_type(self, leaf: str = "int") -> cpp.CppType:
        return cpp.storage(self.cpp(leaf))

    def storage(self, leaf: str = "int") -> str:
        return self.storage_type(leaf).render()

    def factory_spec(
        self, leaf: str = "tracked", *, const: bool = False
    ) -> Factory | None:
        if self.factory_kind is None:
            return None
        return Factory(self.factory_kind, self.element(leaf), const)

    def factory(self, leaf: str = "tracked") -> str | None:
        spec = self.factory_spec(leaf)
        return spec.name if spec is not None else None

    def const_factory(self, leaf: str = "tracked") -> str | None:
        spec = self.factory_spec(leaf, const=True)
        return spec.name if spec is not None else None

    def external_storage_type(self, leaf: str = "int") -> cpp.CppType:
        constructor = self.external_constructor or self.constructor
        return cpp.storage(constructor.apply(self.element(leaf)))

    def external_storage(self, leaf: str = "int") -> str:
        return self.external_storage_type(leaf).render()

    @property
    def owns(self) -> bool:
        wrapper = self.wrapper
        return wrapper is not None and wrapper.has("owning")

    @property
    def external_moves(self) -> int:
        """Whether taking the external instance transfers its leaf."""
        return 1 if self.owns and not self.sharing else 0


def _form(
    name: str,
    kind: str,
    constructor: FormConstructor,
    wrapper: str | None = None,
    *,
    headers: tuple[str, ...] = (),
    value: bool = True,
    lvalue: bool = True,
    rvalue: bool = True,
    pointer: bool = True,
    sharing: bool = False,
    factory: str | None = None,
    external_constructor: FormConstructor | None = None,
    external_leaf_only: bool = False,
    registration_factory: bool = True,
) -> StorageForm:
    return StorageForm(
        name=name,
        kind=kind,
        wrapper_name=wrapper,
        constructor=constructor,
        headers=headers,
        has_value=value,
        has_lvalue=lvalue,
        has_rvalue_reference=rvalue,
        has_pointer=pointer,
        sharing=sharing,
        factory_kind=factory,
        external_constructor=external_constructor,
        external_leaf_only=external_leaf_only,
        registration_factory=registration_factory,
    )


FORMS = (
    _form(
        "plain",
        "value",
        FormConstructor.VALUE,
        external_constructor=FormConstructor.REFERENCE,
        external_leaf_only=True,
    ),
    _form(
        "alternative",
        "variant",
        FormConstructor.VARIANT,
        "variant",
        headers=("variant",),
        factory="variant",
        registration_factory=False,
    ),
    _form(
        "pointer",
        "pointer",
        FormConstructor.POINTER,
        "pointer",
        factory="pointer",
        registration_factory=False,
    ),
    _form(
        "array",
        "array",
        FormConstructor.RAW_ARRAY,
        value=False,
        lvalue=False,
    ),
    _form(
        "bounded_array",
        "array",
        FormConstructor.ARRAY,
        value=False,
        external_leaf_only=True,
    ),
    _form(
        "unique_handle",
        "handle",
        FormConstructor.UNIQUE_HANDLE,
        "unique_ptr",
        headers=("memory",),
        factory="unique",
    ),
    _form(
        "unique_array_handle",
        "array_handle",
        FormConstructor.UNIQUE_ARRAY,
        "unique_array",
        headers=("memory",),
        value=False,
        factory="unique_array",
    ),
    _form(
        "shared_handle",
        "handle",
        FormConstructor.SHARED_HANDLE,
        "shared_ptr",
        headers=("memory",),
        sharing=True,
        factory="shared",
    ),
    _form(
        "shared_array_handle",
        "array_handle",
        FormConstructor.SHARED_ARRAY,
        "shared_array",
        headers=("memory",),
        sharing=True,
        factory="shared_array",
    ),
    _form(
        "optional",
        "optional",
        FormConstructor.OPTIONAL,
        "optional",
        headers=("optional",),
        factory="optional",
    ),
)

SCOPE_BY_NAME = {scope.name: scope for scope in SCOPES}
FORM_BY_NAME = {form.name: form for form in FORMS}
FORM_ENUM = ("unknown", *(form.name for form in FORMS))

CONSUMING_SCOPES = frozenset(scope.name for scope in SCOPES if scope.consumes)
RUNTIME_ONLY_SCOPES = frozenset(
    scope.name for scope in SCOPES if not scope.can_be_static
)

ARRAY_FORMS = frozenset(form.name for form in FORMS if form.kind == "array")
ARRAY_HANDLE_FORMS = frozenset(
    form.name for form in FORMS if form.kind == "array_handle"
)
ARRAY_STORAGE_FORMS = ARRAY_FORMS | ARRAY_HANDLE_FORMS

# The raw dynamic array has no element count, so no scope can instantiate it.
ROUTE_FORM_NAMES = tuple(form.name for form in FORMS if form.name != "array")
