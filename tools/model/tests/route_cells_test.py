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

from tools.model.emit.families.route import (
    ABSENCE_PROBE_GAPS,
    build_rows,
    build_shards,
    render,
    validate,
)
from tools.model.semantics.routes.ledger import supported_categories
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar.catalog import FORMS, SCOPES


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
    content = "".join(render(shard) for shard in build_shards())
    assert content.count("route unexpectedly available") == missing


def test_render_asserts_every_candidate_target_shape(tmp_path: Path) -> None:
    rows = build_rows()
    expected = sum(
        len(row[f"{category}_candidates"])
        for row in rows
        for category in ("value", "object", "rvalue")
    )
    content = "".join(render(shard) for shard in build_shards())
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
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


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
    shards = build_shards()
    assert len(shards) == len(build_rows())
    for shard in shards:
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard['suite']}, routes)" in content
        assert f'RecordProperty("path", "{shard['case_path']}")' in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
