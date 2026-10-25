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

from behavior_cells import build_rows, render, validate
from storage_cells import validate_catalog


def test_behavior_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert len(rows) == 7
    assert len({row["name"] for row in rows}) == len(rows)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[2] / "include")


def test_render_names_every_cell(tmp_path: Path) -> None:
    content = render(tmp_path / "behavior_cells.cpp")
    assert content == render(tmp_path / "behavior_cells.cpp")
    for row in build_rows():
        assert f"behavior_cells, {row['name']}" in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
