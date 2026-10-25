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
    BEHAVIOR_GAPS,
    INTERFACE_CASES,
    REQUEST_COSTS,
    build_rows,
    covered_cells,
    render,
    supported_cells,
    validate,
)
from storage_cells import validate_catalog


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
    rows = [row for row in build_rows() if row.get("interface")]
    assert INTERFACE_CASES
    assert rows
    assert {row["container"] for row in rows} == {"runtime", "static"}
    assert {row["scope"] for row in rows} == {"shared", "unique"}
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
