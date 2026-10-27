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
from tools.model.emit.families.interface_runtime import (
    build_rows,
    render,
    validate,
)
from tools.model.grammar.bindings import binding_for
from tools.model.grammar.traits import MOVE_ONLY, TRAITS_BY_NAME
from tools.model.semantics.support import classify


def test_interface_runtime_catalog_is_non_empty_and_unique() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)


def test_interface_rows_are_classified_supported() -> None:
    for row in build_rows():
        scope, name, request, traits = row["coverage"]
        assert (
            classify(
                scope,
                name,
                binding_for(request),
                request,
                TRAITS_BY_NAME[traits],
            ).outcome
            == "supported"
        )


def test_interface_move_only_rows_use_the_move_only_implementation() -> None:
    rows = build_rows(MOVE_ONLY)
    assert rows
    assert len(rows) <= len(build_rows())
    for row in rows:
        scope, name, request, traits = row["coverage"]
        assert traits == MOVE_ONLY.name
        assert (
            classify(
                scope,
                name,
                binding_for(request),
                request,
                TRAITS_BY_NAME[traits],
            ).outcome
            == "supported"
        )
        assert "move_only_implementation_type" in str(row["registration"])
    content = render(None, MOVE_ONLY)
    assert "TEST(interface_runtime_move_only," in content


def test_interface_render_is_deterministic(tmp_path: Path) -> None:
    content = render(tmp_path / "interface_runtime.cpp")
    assert content == render(tmp_path / "interface_runtime.cpp")


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
