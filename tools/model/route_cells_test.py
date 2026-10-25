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

from route_cells import ABSENCE_PROBE_GAPS, build_rows, render, validate
from route_ledger import supported_categories
from storage_cells import FORMS, SCOPES, validate_catalog


def test_route_product_is_complete() -> None:
    validate()
    rows = build_rows()
    keys = {(row["scope_name"], row["form_name"]) for row in rows}
    assert len(keys) == len(rows)
    assert all(row["form_name"] != "array" for row in rows)


def test_unavailable_categories_have_absence_assertions(tmp_path: Path) -> None:
    rows = build_rows()
    missing = sum(
        1
        for row in rows
        for category in ("value", "object", "rvalue")
        if not row[category]
    )
    content = render(tmp_path / "route_cells.cpp")
    assert content.count("route unexpectedly available") == missing


def test_render_asserts_every_candidate_target_shape(tmp_path: Path) -> None:
    rows = build_rows()
    expected = sum(
        len(row[f"{category}_targets"])
        for row in rows
        for category in ("value", "object", "rvalue")
    )
    content = render(tmp_path / "route_cells.cpp")
    assert content.count("route target shape changed") == expected


def test_every_unavailable_cell_has_an_explicit_probe_gap() -> None:
    expected = {
        (scope.name, form.name)
        for scope in SCOPES
        for form in FORMS
        if not supported_categories(scope.name, form.name)
    }
    assert expected == set(ABSENCE_PROBE_GAPS)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[2] / "include")


def test_route_categories_follow_scope_and_form() -> None:
    rows = {
        (row["scope_name"], row["form_name"]): row for row in build_rows()
    }
    assert rows[("shared", "plain")]["value"] is True
    assert rows[("shared", "plain")]["object"] is True
    assert rows[("shared", "plain")]["rvalue"] is False
    assert rows[("unique", "plain")]["value"] is True
    assert rows[("unique", "plain")]["object"] is True
    assert rows[("unique", "plain")]["rvalue"] is True
    assert rows[("shared_cyclical", "plain")]["value"] is False
    assert rows[("shared_cyclical", "plain")]["object"] is True
    assert rows[("shared_cyclical", "shared_handle")]["value"] is True


def test_render_names_every_cell(tmp_path: Path) -> None:
    content = render(tmp_path / "route_cells.cpp")
    assert content == render(tmp_path / "route_cells.cpp")
    for row in build_rows():
        assert f"{row['scope_name']}_{row['form_name']}" in content
    assert content.count("TEST(route_cells, ") == len(build_rows()) + 1


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
