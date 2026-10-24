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

from selection_rules import (
    POLICIES,
    STATUSES,
    build_rows,
    render,
    resolve,
    validate,
    validate_catalog,
)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[2] / "include")


def test_product_is_complete() -> None:
    validate()
    rows = build_rows()
    assert len(rows) == len(POLICIES) * len(STATUSES) * len(STATUSES)
    assert len({(row.policy, row.primary, row.secondary) for row in rows}) == len(
        rows
    )


def test_preferred_primary_wins_before_secondary() -> None:
    assert resolve("found", "ambiguous", "prefer_primary") == "primary"
    assert resolve("ambiguous", "found", "prefer_primary") == "ambiguous"
    assert resolve("not_found", "found", "prefer_primary") == "secondary"


def test_conflict_policy_rejects_two_sources() -> None:
    assert resolve("found", "found", "ambiguous_on_conflict") == "ambiguous"
    assert resolve("found", "not_found", "ambiguous_on_conflict") == "primary"
    assert resolve("not_found", "found", "ambiguous_on_conflict") == "secondary"


def test_render_names_every_cell(tmp_path: Path) -> None:
    content = render(tmp_path / "selection_rules.cpp")
    assert content == render(tmp_path / "selection_rules.cpp")
    assert content.count("selection policy changed for") == len(build_rows())


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
