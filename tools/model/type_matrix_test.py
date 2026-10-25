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

from storage_cells import validate_catalog
from type_matrix import SHAPES, build_rows, render, validate


def test_type_matrix_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert SHAPES
    assert len({row["cell"] for row in rows}) == len(rows)
    assert len({shape.name for shape in SHAPES}) == len(SHAPES)


def test_type_matrix_derives_wrapper_shapes() -> None:
    cpp_types = {shape.cpp_type for shape in SHAPES}
    assert {
        "int",
        "int &",
        "const int &",
        "int *",
        "const int *",
        "int * *",
        "std::optional<int *>",
        "std::optional<std::optional<int>>",
        "std::shared_ptr<std::optional<int>>",
        "std::optional<std::shared_ptr<int>>",
        "std::shared_ptr<int>",
        "std::unique_ptr<int>",
        "int (&)[2]",
        "int (*)[2]",
    } <= cpp_types
    assert all(shape.depth <= 2 for shape in SHAPES)


def test_type_matrix_cells_are_instantiables() -> None:
    for row in build_rows():
        assert row["type"]
        assert row["constructor"]
        assert row["scope"] in {"shared", "unique", "external", "shared_cyclical"}


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[2] / "include")


def test_render_names_every_cell(tmp_path: Path) -> None:
    content = render(tmp_path / "type_matrix.cpp")
    assert content == render(tmp_path / "type_matrix.cpp")
    for row in build_rows():
        assert f"namespace {row['cell']}" in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
