#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import pytest

from route_ledger import (
    CATEGORIES,
    build_rows,
    category_access,
    category_kinds,
    category_retention,
    gaps,
    supported_categories,
    validate,
)
from storage_cells import FORMS, SCOPES, exposure


def test_every_row_has_one_outcome() -> None:
    validate()
    rows = build_rows()
    assert len(rows) == len(SCOPES) * len(FORMS) * len(CATEGORIES)
    assert len({(row.scope, row.form, row.category) for row in rows}) == len(
        rows
    )


def test_unavailable_rows_have_reasons() -> None:
    for row in build_rows():
        if row.outcome == "unavailable":
            assert row.reason
            assert not row.gap


def test_no_untracked_gaps() -> None:
    assert gaps() == []


def test_raw_dynamic_array_is_unavailable() -> None:
    array_form = next(form for form in FORMS if form.name == "array")
    for scope in SCOPES:
        assert exposure(scope, array_form) == 0
    rows = [row for row in build_rows() if row.form == "array"]
    assert len(rows) == len(SCOPES) * len(CATEGORIES)
    assert {row.outcome for row in rows} == {"unavailable"}
    assert all(row.reason for row in rows)


def test_unsized_cyclical_array_handle_is_unavailable() -> None:
    assert supported_categories("shared_cyclical", "shared_array_handle") == set()
    rows = [
        row
        for row in build_rows()
        if row.scope == "shared_cyclical" and row.form == "shared_array_handle"
    ]
    assert {row.outcome for row in rows} == {"unavailable"}
    assert all(row.reason for row in rows)


def test_supported_categories_match_the_scope_form_product() -> None:
    assert supported_categories("shared", "plain") == {"value", "object"}
    assert supported_categories("unique", "plain") == {
        "value",
        "object",
        "rvalue",
    }
    assert supported_categories("shared_cyclical", "plain") == {"object"}
    assert supported_categories("shared_cyclical", "optional") == set()
    assert supported_categories("shared", "array") == set()
    assert supported_categories("shared", "bounded_array") == {"object"}
    assert supported_categories("unique", "bounded_array") == {
        "value",
        "object",
        "rvalue",
    }
    assert supported_categories("shared_cyclical", "bounded_array") == {
        "object"
    }
    assert supported_categories("shared", "shared_array_handle") == {
        "value",
        "object",
    }


def test_access_follows_scope_and_category() -> None:
    assert category_access("shared", "plain", "value") == "borrow"
    assert category_access("shared", "plain", "object") == "borrow"
    assert category_access("unique", "plain", "value") == "consume"
    assert category_access("unique", "plain", "object") == "borrow"
    assert category_access("unique", "plain", "rvalue") == "consume"
    assert category_access("external", "plain", "value") == "borrow"


def test_plain_operation_kinds_are_modeled() -> None:
    assert category_kinds("shared", "plain", "value") == ("dereference",)
    assert category_kinds("shared", "plain", "object") == ("traits",)
    assert category_kinds("unique", "plain", "value") == (
        "identity",
        "wrapper",
    )
    assert category_kinds("external", "plain", "object") == ("address",)
    assert category_kinds("shared_cyclical", "plain", "object") == (
        "traits",
    )


def test_retention_is_modeled_for_every_supported_route() -> None:
    for row in build_rows():
        if row.outcome == "supported":
            assert len(row.retention) == len(row.kinds)
            assert all(value in (0, 1) for value in row.retention)
    assert category_retention("shared", "shared_handle", "value") == (1, 1, 0)
    assert category_retention("unique", "plain", "value") == (0, 1)
    assert category_retention("shared", "optional", "object") == (1, 0, 1, 0)
    assert category_retention("shared_cyclical", "pointer", "object") == (
        1,
        1,
    )


def test_all_supported_routes_have_operation_kinds() -> None:
    for row in build_rows():
        if row.outcome == "supported":
            assert row.kinds
            assert not row.kind_gap


def test_non_plain_operation_kinds_are_modeled() -> None:
    assert category_kinds("shared", "shared_handle", "value") == (
        "borrowed",
        "traits",
        "identity",
    )
    assert category_kinds("shared", "shared_handle", "object") == (
        "address",
        "borrowed",
        "pointer_access",
    )
    assert category_kinds("unique", "alternative", "value") == (
        "alternative",
    )
    assert category_kinds("shared_cyclical", "pointer", "object") == (
        "dereference",
        "borrowed",
    )


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
