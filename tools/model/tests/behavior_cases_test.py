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

from tools.model.emit.families.behavior import (
    ANNOTATION_GAPS,
    ANNOTATION_RECIPES,
    ANNOTATION_SHAPES,
    BEHAVIOR_GAPS,
    FORMS,
    INTERFACE_CASES,
    INTERFACE_GAPS,
    INTERFACE_SHAPES,
    REQUEST_COSTS,
    build_rows,
    build_shards,
    covered_forms,
    derive_expectation,
    derive_extra_expectations,
    render,
    supported_forms,
    validate,
)
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar.traits import MOVE_ONLY


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


def test_extra_route_cases_are_derived() -> None:
    unique_plain = derive_expectation(_form("plain"), "unique")
    extras = derive_extra_expectations(_form("plain"), "unique", unique_plain)
    assert [case for case, _ in extras] == ["rvalue"]
    assert extras[0][1].request == "tracked &&"
    assert extras[0][1].move_out is True

    shared_variant = derive_expectation(_form("alternative"), "shared")
    extras = derive_extra_expectations(
        _form("alternative"), "shared", shared_variant
    )
    assert [case for case, _ in extras] == ["wrapper"]
    assert extras[0][1].request == "std::variant<tracked, long>"


def test_every_non_gap_supported_form_derives_a_known_request_class() -> None:
    for form in FORMS:
        for scope in ("shared", "unique", "external", "shared_cyclical"):
            if (scope, form.name) in BEHAVIOR_GAPS:
                continue
            if (scope, form.name) not in supported_forms():
                continue
            expectation = derive_expectation(form, scope)
            assert expectation.request_class in REQUEST_COSTS


def test_behavior_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)


def test_every_supported_form_has_a_behavior_case_or_gap() -> None:
    accounted = covered_forms() | set(BEHAVIOR_GAPS)
    assert accounted == supported_forms()


def test_behavior_gaps_have_reasons() -> None:
    for form, reason in BEHAVIOR_GAPS.items():
        assert form in supported_forms()
        assert reason


def test_interface_cases_are_derived_from_shapes() -> None:
    assert len(INTERFACE_SHAPES) == 6
    names = {case.name for case in INTERFACE_CASES}
    assert {
        "shared_ref",
        "shared_ptr",
        "shared_handle",
        "shared_plain_ref",
        "unique_handle",
        "unique_shared_handle",
        "shared_second_ref",
        "shared_second_handle",
        "unique_second_handle",
        "shared_copyable_value",
    } == names
    by_name = {case.name: case for case in INTERFACE_CASES}
    assert by_name["unique_handle"].request == (
        "std::unique_ptr<tracked_interface>"
    )
    assert by_name["shared_second_handle"].request == (
        "std::shared_ptr<tracked_interface2>"
    )
    assert by_name["shared_copyable_value"].request_class == "borrow_value"


def test_interface_shapes_cover_forms_or_declare_gaps() -> None:
    covered = {shape.form_name for shape in INTERFACE_SHAPES}
    assert covered | set(INTERFACE_GAPS) == {form.name for form in FORMS}
    assert covered & set(INTERFACE_GAPS) == set()


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


def test_annotation_recipes_are_derived_from_shapes() -> None:
    assert len(ANNOTATION_SHAPES) == 9
    names = {recipe.name for recipe in ANNOTATION_RECIPES}
    assert {
        "shared_annotated_ref",
        "shared_annotated_value",
        "shared_annotated_shared_ptr_ref",
        "shared_annotated_optional_ref",
        "shared_annotated_pointer_ref",
        "shared_annotated_interface_ref",
        "shared_annotated_interface_ptr",
        "shared_annotated_interface_handle",
        "shared_annotated_interface_handle_ref",
        "external_annotated_value",
        "external_annotated_ref",
        "external_annotated_ptr",
        "external_annotated_shared_ptr",
    } == names
    by_name = {recipe.name: recipe for recipe in ANNOTATION_RECIPES}
    assert by_name["shared_annotated_interface_handle"].request_class == "handle"
    assert by_name["external_annotated_value"].registration_moves == 1
    assert by_name["external_annotated_ptr"].external_argument == (
        "std::addressof(external_instance)"
    )


def test_annotation_shapes_cover_forms_or_declare_gaps() -> None:
    covered = {
        "plain",
        "shared_handle",
        "optional",
        "pointer",
    }
    assert covered | set(ANNOTATION_GAPS) == {form.name for form in FORMS}
    assert covered & set(ANNOTATION_GAPS) == set()


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


def test_external_forms_are_runtime_only() -> None:
    for row in build_rows():
        if row["scope"] == "external":
            assert row["container"] == "runtime"
            assert row["instance"]


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


def test_move_only_expectations_borrow_or_move() -> None:
    shared_plain = derive_expectation(_form("plain"), "shared", MOVE_ONLY)
    assert shared_plain.request == "move_only_tracked &"
    assert shared_plain.request_class == "reference"
    unique_plain = derive_expectation(_form("plain"), "unique", MOVE_ONLY)
    assert unique_plain.request == "move_only_tracked"
    assert unique_plain.request_class == "consume_value"
    shared_pointer = derive_expectation(_form("pointer"), "shared", MOVE_ONLY)
    assert shared_pointer.request == "move_only_tracked *"
    assert shared_pointer.request_class == "reference"
    shared_optional = derive_expectation(_form("optional"), "shared", MOVE_ONLY)
    assert shared_optional.request == "std::optional<move_only_tracked> &"


def test_move_only_rows_have_no_copying_expectation() -> None:
    rows = build_rows(MOVE_ONLY)
    assert rows
    copies = [
        row["copies"]
        for row in rows
        if not row.get("interface") and not row.get("annotation")
    ]
    assert copies and set(copies) == {0}
    assert any("move_only_tracked" in str(row["request"]) for row in rows)
    content = "".join(
        render(shard)
        for shard in build_shards()
        if shard.traits == MOVE_ONLY.name
    )
    assert "struct move_only_tracked" in content
    assert "move_only_tracked(const move_only_tracked &) = delete;" in content


def test_render_names_every_form(tmp_path: Path) -> None:
    shards = build_shards()
    assert shards
    for shard in shards:
        content = render(shard)
        assert content == render(shard)
        for row in shard.rows:
            assert f"{row['name']}" in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
