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

from behavior_cells import (
    ANNOTATION_RECIPES,
    BEHAVIOR_GAPS,
    FORMS,
    INTERFACE_CASES,
    REQUEST_COSTS,
    build_rows,
    covered_cells,
    derive_expectation,
    render,
    supported_cells,
    validate,
)
from storage_cells import validate_catalog


def _form(name: str):
    return next(form for form in FORMS if form.name == name)


def test_expectations_follow_the_semantic_form_kind() -> None:
    shared_handle = derive_expectation(_form("shared_handle"), "shared")
    assert shared_handle.request == "std::shared_ptr<tracked>"
    assert shared_handle.request_class == "handle"
    assert shared_handle.leaf == "pointer"

    unique_handle = derive_expectation(_form("unique_handle"), "unique")
    assert unique_handle.request == "std::unique_ptr<tracked>"
    assert unique_handle.request_class == "handle"

    unique_array = derive_expectation(_form("bounded_array"), "unique")
    assert unique_array.request == "tracked *"
    assert unique_array.request_class == "array"
    assert unique_array.delete_result == "delete[]"
    assert unique_array.constructions == 4

    cyclical_array = derive_expectation(
        _form("bounded_array"), "shared_cyclical"
    )
    assert cyclical_array.request == "tracked (*)[2]"
    assert cyclical_array.constructions == 2

    external = derive_expectation(_form("plain"), "external")
    assert external.constructions == 0
    assert external.leaf == "external"


def test_every_non_gap_supported_cell_derives_a_known_request_class() -> None:
    for form in FORMS:
        for scope in ("shared", "unique", "external", "shared_cyclical"):
            if (scope, form.name) in BEHAVIOR_GAPS:
                continue
            if (scope, form.name) not in supported_cells():
                continue
            expectation = derive_expectation(form, scope)
            assert expectation.request_class in REQUEST_COSTS


def test_behavior_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)


def test_every_supported_cell_has_a_behavior_case_or_gap() -> None:
    accounted = covered_cells() | set(BEHAVIOR_GAPS)
    assert accounted == supported_cells()


def test_behavior_gaps_have_reasons() -> None:
    for cell, reason in BEHAVIOR_GAPS.items():
        assert cell in supported_cells()
        assert reason


def test_interface_cases_cover_both_containers() -> None:
    rows = [
        row
        for row in build_rows()
        if row.get("interface") and not row.get("annotation")
    ]
    assert INTERFACE_CASES
    assert rows
    assert {row["container"] for row in rows} == {"runtime", "static"}
    assert {row["scope"] for row in rows} == {"shared", "unique"}
    assert all(row["request_class"] in REQUEST_COSTS for row in rows)


def test_annotation_recipes_are_valid() -> None:
    rows = [row for row in build_rows() if row.get("annotation")]
    assert ANNOTATION_RECIPES
    assert rows
    assert len({recipe.name for recipe in ANNOTATION_RECIPES}) == len(
        ANNOTATION_RECIPES
    )
    assert {row["scope"] for row in rows} == {"shared", "external"}
    assert {
        row["container"] for row in rows if row["scope"] == "external"
    } == {"runtime"}
    assert all(row["request_class"] in REQUEST_COSTS for row in rows)


def test_external_cells_are_runtime_only() -> None:
    for row in build_rows():
        if row["scope"] == "external":
            assert row["container"] == "runtime"
            assert row["instance"]


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[2] / "include")


def test_render_names_every_cell(tmp_path: Path) -> None:
    content = render(tmp_path / "behavior_cells.cpp")
    assert content == render(tmp_path / "behavior_cells.cpp")
    for row in build_rows():
        assert f"behavior_cells, {row['name']}" in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
