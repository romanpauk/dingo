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
    GAP_ARRAY_ROUTES,
    GAP_OPERATION_KIND,
    build_rows,
    category_access,
    category_kinds,
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


def test_gaps_are_explicit_and_tracked() -> None:
    gap_rows = gaps()
    assert gap_rows
    assert {row.gap for row in gap_rows} == {GAP_ARRAY_ROUTES}
    for row in gap_rows:
        assert exposure(
            next(scope for scope in SCOPES if scope.name == row.scope),
            next(form for form in FORMS if form.name == row.form),
        ) != 0


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


def test_non_plain_operation_kinds_are_tracked_gaps() -> None:
    for row in build_rows():
        if row.outcome == "supported" and row.form != "plain":
            assert row.kind_gap == GAP_OPERATION_KIND
            assert row.kinds == ()


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
