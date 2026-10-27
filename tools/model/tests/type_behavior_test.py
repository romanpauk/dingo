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

from tools.model.emit.families.behavior import BEHAVIOR_GAPS, supported_categories
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar.catalog import FORMS, SCOPES
from tools.model.emit.behavior import (
    TOP_LEVEL_BORROW_GAP,
    build_rows,
    delete_kind,
    render,
    validate,
)
from tools.model.emit.matrix import SHAPES
from tools.model.grammar.traits import MOVE_ONLY


def test_type_behavior_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)
    for row in rows:
        if row["container"] == "runtime":
            assert row["registration"]
            assert not row["static_source"]
        else:
            assert row["static_source"]
            assert not row["registration"]
        assert len(row["shapes"]) == len(SHAPES)
        assert any(not shape["gap"] for shape in row["shapes"])


def test_type_behavior_cells_follow_the_route_ledger() -> None:
    cells = {(row["scope"], row["form"], row["container"]) for row in build_rows()}
    expected = {
        (scope.name, form.name, container)
        for scope in SCOPES
        for form in FORMS
        if supported_categories(scope.name, form.name)
        and (scope.name, form.name) not in BEHAVIOR_GAPS
        for container in ("runtime", "static")
        if not (scope.name == "external" and container == "static")
    }
    assert cells == expected


def test_consuming_scope_lvalue_borrows_are_top_level_gaps() -> None:
    lvalue_shapes = {
        shape.name for shape in SHAPES if shape.is_lvalue_reference
    }
    for row in build_rows():
        for shape in row["shapes"]:
            if row["scope"] == "unique" and shape["name"] in lvalue_shapes:
                assert shape["gap"] == TOP_LEVEL_BORROW_GAP
            else:
                assert not shape["gap"]


def test_consuming_scope_raw_pointers_transfer_ownership() -> None:
    rows = {row["name"]: row for row in build_rows()}
    pointer = rows["unique_pointer_runtime"]
    delete_kinds = {
        shape["name"]: shape["delete"]
        for shape in pointer["shapes"]
        if not shape["gap"]
    }
    assert delete_kinds["pointer_value"] == "single"
    assert delete_kinds["pointer_const_value"] == "single"

    bounded_array = rows["unique_bounded_array_runtime"]
    delete_kinds = {
        shape["name"]: shape["delete"]
        for shape in bounded_array["shapes"]
        if not shape["gap"]
    }
    assert delete_kinds["pointer_value"] == "array"
    assert delete_kinds["array_pointer_value"] == "array"
    assert delete_kinds["unique_array_value"] == "none"
    assert delete_kinds["shared_array_value"] == "none"

    assert delete_kind("shared", "bounded_array", SHAPES[0]) == "none"
    pointer_shape = next(shape for shape in SHAPES if shape.is_pointer)
    assert delete_kind("shared", "pointer", pointer_shape) == "none"
    assert delete_kind("external", "pointer", pointer_shape) == "none"


def test_move_only_rows_use_the_move_only_leaf() -> None:
    rows = build_rows(("runtime",), MOVE_ONLY)
    assert rows
    plain = next(
        row
        for row in rows
        if row["scope"] == "shared" and row["form"] == "plain"
    )
    assert plain["storage_type"] == "move_only_type"
    assert plain["constructor"] == "move_only_type"
    content = render(rows, MOVE_ONLY)
    assert "TEST(type_behavior_move_only," in content
    assert "struct move_only_type" in content
    assert "move_only_type(const move_only_type &) = delete;" in content
    assert "std::optional<move_only_type>" in content


def test_trait_rows_keep_the_same_cells() -> None:
    from tools.model.grammar.traits import COPYABLE

    copyable = build_rows(("runtime",), COPYABLE)
    move_only = build_rows(("runtime",), MOVE_ONLY)
    assert [(row["scope"], row["form"], row["container"]) for row in copyable] == [
        (row["scope"], row["form"], row["container"]) for row in move_only
    ]


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


def test_render_is_stable_and_covers_every_cell(tmp_path: Path) -> None:
    for container, name in (("runtime", "type_behavior.cpp"),
                            ("static", "type_behavior_static.cpp")):
        rows = build_rows((container,))
        content = render(rows)
        assert content == render(rows)
        assert f"/{name}" not in content
        for row in rows:
            assert f"TEST(type_behavior, {row['name']})" in content
        assert "top-level gap:" in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
