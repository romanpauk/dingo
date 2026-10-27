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

from tools.model.emit.families.interface import INTERFACE_ROUTES, render, validate
from tools.model.emit.common.validation import validate_catalog


def test_interface_route_product_is_complete() -> None:
    validate()
    assert INTERFACE_ROUTES
    assert len({route.name for route in INTERFACE_ROUTES}) == len(
        INTERFACE_ROUTES
    )


def test_interface_routes_cover_both_scopes() -> None:
    assert {route.scope for route in INTERFACE_ROUTES} == {"shared", "unique"}
    assert any(route.value for route in INTERFACE_ROUTES)
    assert any(route.object for route in INTERFACE_ROUTES)
    assert any(route.rvalue for route in INTERFACE_ROUTES)


def test_interface_route_counts_match_availability() -> None:
    for route in INTERFACE_ROUTES:
        assert (route.value_count != 0) == route.value
        assert (route.object_count != 0) == route.object
        assert (route.rvalue_count != 0) == route.rvalue


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


def test_render_names_every_cell(tmp_path: Path) -> None:
    content = render(tmp_path / "interface_cells.cpp")
    assert content == render(tmp_path / "interface_cells.cpp")
    for route in INTERFACE_ROUTES:
        assert f"namespace {route.name}" in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
