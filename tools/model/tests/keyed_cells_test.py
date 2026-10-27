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

from tools.model.emit.common.validation import validate_catalog
from tools.model.emit.families.keyed import build_rows, render, validate
from tools.model.grammar.bindings import KEYED
from tools.model.grammar.traits import MOVE_ONLY
from tools.model.semantics.support import classify


def test_keyed_catalog_is_non_empty_and_unique() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)


def test_keyed_rows_are_classified_supported() -> None:
    for row in build_rows():
        scope, name, request = row["coverage"]
        assert (
            classify(scope, name, KEYED, request).outcome == "supported"
        )


def test_keyed_move_only_rows_use_the_move_only_leaf() -> None:
    rows = build_rows(MOVE_ONLY)
    assert rows
    assert len(rows) < len(build_rows())
    for row in rows:
        scope, name, request = row["coverage"]
        assert (
            classify(scope, name, KEYED, request, MOVE_ONLY).outcome
            == "supported"
        )
        assert "move_only_tracked" in str(row["request"])
        assert "move_only_tracked" in str(row["registration"])
    assert any(
        "make_shared_move_only_tracked" in str(row["registration"])
        for row in rows
    )


def test_keyed_render_is_deterministic(tmp_path: Path) -> None:
    content = render(tmp_path / "keyed_cells.cpp")
    assert content == render(tmp_path / "keyed_cells.cpp")


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
