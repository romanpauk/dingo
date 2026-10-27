#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from pathlib import Path

import pytest

from tools.model.emit.common.validation import validate_catalog
from tools.model.emit.matrix import (
    BASE_SHAPES,
    SHAPES,
    build_rows,
    gap_reason,
    render,
    shape_family,
    top_level_safe,
    validate,
)


def test_type_matrix_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert SHAPES
    assert len({row["path"] for row in rows}) == len(rows)
    assert len({shape.name for shape in SHAPES}) == len(SHAPES)
    for row in rows:
        assert set(row["reasons"]) == {shape.name for shape in SHAPES}


def test_type_matrix_derives_wrapper_shapes() -> None:
    cpp_types = {shape.cpp_type for shape in SHAPES}
    assert {
        "int",
        "int &",
        "const int &",
        "int *",
        "const int *",
        "int * *",
        "std::optional<int *>",
        "std::optional<std::optional<int>>",
        "std::shared_ptr<std::optional<int>>",
        "std::optional<std::shared_ptr<int>>",
        "std::shared_ptr<int>",
        "std::unique_ptr<int>",
        "int (&)[2]",
        "int (*)[2]",
        "std::variant<int, long>",
        "std::optional<std::variant<int, long>>",
    } <= cpp_types
    assert all(shape.depth <= 2 for shape in BASE_SHAPES)
    assert set(shape.name for shape in BASE_SHAPES) <= {
        shape.name for shape in SHAPES
    }


def test_type_matrix_forms_are_instantiables() -> None:
    for row in build_rows():
        assert row["type"]
        assert row["constructor"]
        assert row["scope"] in {"shared", "unique", "external", "shared_cyclical"}


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


def test_shape_families_name_every_composition() -> None:
    families = {shape.name: shape_family(shape) for shape in SHAPES}
    assert families["optional_optional_value"] == "nested wrapper"
    assert (
        families["optional_pointer_value"]
        == "access form inside a wrapper"
    )
    assert (
        families["shared_ptr_const_value"]
        == "const-qualified wrapper element"
    )
    assert (
        families["pointer_pointer_value"]
        == "reference or pointer to an access form"
    )
    assert families["array_pointer_value"] == "array view"
    assert families["value"] == ""
    assert families["lvalue_reference_value"] == ""
    assert families["shared_ptr_value"] == ""


def test_unsupported_shapes_get_a_route_rule() -> None:
    rules = {
        "access",
        "borrow",
        "move",
        "copy_out",
        "take",
        "wrap",
        "share",
        "adopt",
        "decay",
        "select_alternative",
        "nested_access",
        "const_copy",
        "target",
    }
    assert gap_reason("shared", "plain", SHAPES[0])
    for row in build_rows():
        for shape in SHAPES:
            reason = row["reasons"][shape.name]
            assert reason
            rule = reason.split(":", 1)[0]
            assert rule in rules, (row["scope"], row["form"], shape.name, reason)

    by_name = {shape.name: shape for shape in SHAPES}
    assert gap_reason("shared", "plain", by_name["rvalue_reference_value"]).startswith(
        "move:"
    )
    assert gap_reason("unique", "plain", by_name["pointer_value"]).startswith(
        "borrow:"
    )
    assert gap_reason("shared", "plain", by_name["array_pointer_value"]).startswith(
        "decay:"
    )
    assert gap_reason("shared", "plain", by_name["pointer_pointer_value"]).startswith(
        "nested_access:"
    )
    assert gap_reason("shared", "plain", by_name["optional_optional_value"]).startswith(
        "wrap:"
    )


def test_render_names_every_form(tmp_path: Path) -> None:
    content = render(tmp_path / "type_matrix.cpp")
    assert content == render(tmp_path / "type_matrix.cpp")
    for row in build_rows():
        assert f"namespace {row['path']}" in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
