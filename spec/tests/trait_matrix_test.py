#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.domain.cases import iter_cases, request_forms
from spec.emit.matrix import SHAPES
from spec.emit.traits import probe_shape, build_rows, render, validate
from spec.grammar.bindings import CONCRETE
from spec.grammar.expr import leaf, lvalue_reference, pointer
from spec.grammar.traits import (
    COPYABLE,
    MOVE_ONLY,
    TRAITS,
    TRAITS_BY_NAME,
)
from spec.semantics.support import classify


def _row(scope: str, form: str) -> dict[str, object]:
    return next(
        row
        for row in build_rows()
        if row["scope"] == scope and row["form"] == form
    )


def test_traits_are_a_bounded_pair() -> None:
    assert [traits.name for traits in TRAITS] == ["copyable", "move_only"]
    assert TRAITS_BY_NAME["copyable"] is COPYABLE
    assert TRAITS_BY_NAME["move_only"] is MOVE_ONLY
    assert COPYABLE.copyable and COPYABLE.movable
    assert not MOVE_ONLY.copyable and MOVE_ONLY.movable
    assert COPYABLE.cpp != MOVE_ONLY.cpp


def test_matrix_is_complete_and_deterministic() -> None:
    validate()
    first = build_rows()
    second = build_rows()
    assert first == second
    assert first
    names = [row["name"] for row in first]
    assert len(names) == len(set(names))
    for row in first:
        assert row["copyable_storage"]
        assert row["move_only_storage"]
        names = [shape["name"] for shape in row["shapes"]]
        assert names == [
            shape.name for shape in SHAPES if probe_shape(shape)
        ]
        assert names


def test_both_storage_spellings_are_built() -> None:
    row = _row("shared", "plain")
    assert row["copyable_storage"] == "tracked"
    assert row["move_only_storage"] == "move_only_tracked"
    handle = _row("shared", "shared_handle")
    assert handle["copyable_storage"] == "std::shared_ptr<tracked>"
    assert handle["move_only_storage"] == "std::shared_ptr<move_only_tracked>"


def test_value_requests_copy_but_borrows_do_not() -> None:
    shared_value = _shape(_row("shared", "plain"), "value")
    assert shared_value["copies"] is True
    shared_reference = _shape(_row("shared", "plain"), "lvalue_reference_value")
    assert shared_reference["copies"] is False
    shared_pointer = _shape(_row("shared", "plain"), "pointer_value")
    assert shared_pointer["copies"] is False

    # A consuming scope moves its value, so the same request does not copy.
    unique_value = _shape(_row("unique", "plain"), "value")
    assert unique_value["copies"] is False


def test_handles_copy_the_handle_not_the_leaf() -> None:
    shared = _row("shared", "shared_handle")
    assert _shape(shared, "shared_ptr_value")["copies"] is False
    assert _shape(shared, "value")["copies"] is True
    # Wrapping the stored handle in an optional copies the handle.
    assert (
        _shape(shared, "optional_shared_ptr_value")["copies"] is False
    )


def test_const_pointee_forces_a_copy_only_when_promoted() -> None:
    # Promoting a stored value into a const-pointee handle constructs the
    # pointee, so it copies a move-only leaf.
    assert _shape(_row("unique", "plain"), "shared_ptr_const_value")["copies"]
    # A stored shared handle is merely converted, so the leaf is untouched.
    assert not _shape(
        _row("shared", "shared_handle"), "shared_ptr_const_value"
    )["copies"]
    # An adopted pointer is not a promoted value.
    assert not _shape(
        _row("unique", "pointer"), "shared_ptr_const_value"
    )["copies"]


def test_move_only_classification_removes_exactly_the_copying_routes() -> None:
    base = leaf("T")
    shared_values = ("shared", "plain")
    copyable = classify(*shared_values, CONCRETE, base, COPYABLE)
    move_only = classify(*shared_values, CONCRETE, base, MOVE_ONLY)
    assert copyable.outcome == "supported"
    assert move_only.outcome == "unsupported"
    assert move_only.rule == "copy"
    assert move_only.reason

    for request in (lvalue_reference(base), pointer(base)):
        assert (
            classify(*shared_values, CONCRETE, request, MOVE_ONLY).outcome
            == "supported"
        )

    unique_move = classify("unique", "plain", CONCRETE, base, MOVE_ONLY)
    assert unique_move.outcome == "supported"


def test_cases_carry_traits() -> None:
    copyable = iter_cases(request_forms(), COPYABLE)
    move_only = iter_cases(request_forms(), MOVE_ONLY)
    assert copyable
    assert len(copyable) == len(move_only)
    assert {case.traits for case in copyable} == {COPYABLE}
    assert {case.traits for case in move_only} == {MOVE_ONLY}


def test_rendered_matrix_asserts_both_traits() -> None:
    content = render(None)
    assert "namespace trait_matrix" in content
    assert "struct tracked" in content
    assert "struct move_only_tracked" in content
    assert "tracked(const tracked &) = delete" not in content
    assert "move_only_tracked(const move_only_tracked &) = delete" in content
    assert content.count("static_assert") > 0
    assert "copyable_storage" in content
    assert "move_only_storage" in content


def _shape(row: dict[str, object], name: str) -> dict[str, object]:
    return next(shape for shape in row["shapes"] if shape["name"] == name)
