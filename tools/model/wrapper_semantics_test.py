#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import pytest

from wrapper_semantics import (
    BASE,
    SHAPES,
    WRAPPER_BY_NAME,
    form_family,
    form_wrapper,
)


def test_shape_catalog_is_derived_from_wrappers() -> None:
    assert SHAPES
    assert BASE.name == "value"
    assert len({shape.name for shape in SHAPES}) == len(SHAPES)
    assert all(shape.depth <= 2 for shape in SHAPES)
    assert {shape.cpp_type for shape in SHAPES} >= {
        "int",
        "int &",
        "const int &",
        "int *",
        "const int *",
        "int * *",
        "std::optional<int>",
        "std::optional<int *>",
        "std::optional<std::optional<int>>",
        "std::shared_ptr<int>",
        "std::unique_ptr<int>",
        "std::variant<int, long>",
        "std::optional<std::variant<int, long>>",
        "int (&)[2]",
        "int (*)[2]",
    }


def test_custom_wrapper_shapes_are_generated() -> None:
    names = {shape.name for shape in SHAPES}
    assert {
        "test_optional_value",
        "test_optional_optional_value",
        "optional_test_optional_value",
        "test_shared_value",
        "test_shared_shared_ptr_value",
        "shared_ptr_test_shared_value",
        "test_unique_value",
        "test_unique_unique_ptr_value",
        "unique_ptr_test_unique_value",
        "test_variant_value",
        "test_variant_variant_value",
        "variant_test_variant_value",
    } <= names
    assert all(shape.depth <= 2 for shape in SHAPES)


def test_shape_properties_come_from_the_wrapper() -> None:
    optional = WRAPPER_BY_NAME["optional"]
    shared = WRAPPER_BY_NAME["shared_ptr"]
    unique = WRAPPER_BY_NAME["unique_ptr"]
    variant = WRAPPER_BY_NAME["variant"]

    assert {"owning", "nullable", "borrowable"} <= optional.properties
    assert {"owning", "sharing"} <= shared.properties
    assert {"owning", "sole_owner"} <= unique.properties
    assert "variant" in variant.properties

    optional_shape = BASE.apply(optional)
    assert optional_shape.access == "value"
    assert optional_shape.has("nullable")
    assert optional_shape.is_reference is False

    reference = BASE.apply(WRAPPER_BY_NAME["lvalue_reference"])
    assert reference.access == "lvalue_reference"
    assert reference.is_lvalue_reference


def test_wrapper_applicability_rejects_invalid_compositions() -> None:
    reference = BASE.apply(WRAPPER_BY_NAME["lvalue_reference"])
    array_reference = BASE.apply(WRAPPER_BY_NAME["array_lvalue_reference"])
    const = BASE.apply(WRAPPER_BY_NAME["const"])

    assert not WRAPPER_BY_NAME["pointer"].applicable(reference)
    assert not WRAPPER_BY_NAME["optional"].applicable(reference)
    assert not WRAPPER_BY_NAME["const"].applicable(reference)
    assert not WRAPPER_BY_NAME["const"].applicable(const)
    assert not WRAPPER_BY_NAME["const"].applicable(array_reference)
    assert WRAPPER_BY_NAME["const"].applicable(BASE.apply(WRAPPER_BY_NAME["optional"]))


def test_storage_forms_map_to_wrapper_families() -> None:
    assert form_family("plain") == "leaf"
    assert form_family("alternative") == "variant"
    assert form_family("pointer") == "pointer"
    assert form_family("bounded_array") == "array"
    assert form_family("unique_handle") == "handle"
    assert form_family("shared_array_handle") == "array_handle"
    assert form_family("optional") == "optional"

    assert form_wrapper("optional").name == "optional"
    assert form_wrapper("unique_handle").name == "unique_ptr"
    assert form_wrapper("shared_array_handle").name == "shared_array"
    assert form_wrapper("plain") is None


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
