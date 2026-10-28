#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Invariants that keep the constructor registry and its consumers aligned.

The grammar registry is the single place that names constructors. These tests
fail when a constructor is added to the registry but not consumed by a
projection, a shape, a binding, or the storage form map. They do not prove that
the *semantics* of a new constructor are implemented; they prove it cannot be
silently omitted.
"""

from __future__ import annotations

from spec.grammar.bindings import binding_for
from spec.grammar.catalog import SCOPES
from spec.grammar.enumerate import request_types, storage_types
from spec.grammar.storage import storable
from spec.semantics.support import to_shape
from spec.grammar.expr import (
    CONSTRUCTOR_BY_NAME,
    CONSTRUCTORS,
    INDIRECTION_OPS,
    constructors,
    leaf,
)
from spec.grammar.shapes import WRAPPER_BY_NAME


def test_registry_names_are_unique_and_callable() -> None:
    assert len(CONSTRUCTOR_BY_NAME) == len(CONSTRUCTORS)
    for spec in CONSTRUCTORS:
        assert callable(spec.make), spec.name


def test_every_constructor_appears_in_the_request_projection() -> None:
    ops = {expr.op for expr in request_types()}
    missing = {spec.name for spec in CONSTRUCTORS} - ops
    assert not missing, f"constructors missing from request_types: {missing}"


def test_storage_projection_only_uses_registered_constructors() -> None:
    registered = set(CONSTRUCTOR_BY_NAME)
    for expr in storage_types():
        assert expr.op in registered


def test_every_storage_expression_has_a_storage_form() -> None:
    for expr in storage_types():
        forms = [storable(scope, expr) for scope in SCOPES]
        assert any(form is not None for form in forms), expr.render()


def test_wrapper_semantics_covers_wrapper_like_constructors() -> None:
    aliases = {"array_reference": "array_lvalue_reference"}
    for kind in ("qualifier", "wrapper", "variant"):
        for spec in constructors(kind):
            name = aliases.get(spec.name, spec.name)
            assert name in WRAPPER_BY_NAME, spec.name


def test_shape_mapping_covers_wrapper_like_constructors() -> None:
    from spec.semantics.support import _SHAPE_WRAPPERS

    for kind in ("qualifier", "wrapper"):
        for spec in constructors(kind):
            assert spec.name in _SHAPE_WRAPPERS, spec.name


def test_indirection_ops_come_from_the_registry() -> None:
    assert INDIRECTION_OPS <= set(CONSTRUCTOR_BY_NAME)
    expected = {
        spec.name
        for spec in constructors("qualifier") + constructors("wrapper")
        if spec.indirection
    }
    assert INDIRECTION_OPS == expected


def test_every_binding_constructor_has_a_binding() -> None:
    for spec in constructors("binding"):
        expr = spec.make(leaf("T"))
        binding = binding_for(expr)
        assert binding.kind == spec.name, spec.name


def test_forms_expose_structured_cpp_types() -> None:
    from spec.grammar import cpp
    from spec.grammar.catalog import FORM_BY_NAME

    form = FORM_BY_NAME["shared_handle"]
    storage = form.storage_type("tracked")
    assert storage.kind is cpp.CppKind.TEMPLATE
    assert storage.name == "dingo::storage"
    assert storage.args[0].kind is cpp.CppKind.TEMPLATE
    assert storage.args[0].name == "std::shared_ptr"
    assert not hasattr(form, "storage_element")


def test_cpp_types_render_from_structure_not_text() -> None:
    from spec.grammar import cpp

    assert cpp.render(cpp.const(cpp.pointer(cpp.leaf("T")))) == "T * const"
    assert (
        cpp.render(cpp.template("std::shared_ptr", cpp.const(cpp.leaf("T"))))
        == "std::shared_ptr<const T>"
    )
    assert (
        cpp.render(cpp.with_leaf(cpp.leaf("T"), "tracked"))
        == "tracked"
    )


def test_every_request_type_has_a_shape_probe() -> None:
    # `None` means "outside the shape vocabulary", which is a declared model
    # gap. The point here is that lookup itself never raises for a generated
    # type.
    for expr in request_types():
        to_shape(expr)
