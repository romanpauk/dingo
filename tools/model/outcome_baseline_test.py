#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import json
from pathlib import Path

import pytest

from outcome_baseline import build_outcomes, compare, read_matrix


def write_matrix(path: Path, rows: list[dict[str, object]]) -> None:
    path.write_text(
        "\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8"
    )


def test_matrix_rows_are_keyed_by_cell_and_shape(tmp_path: Path) -> None:
    matrix = tmp_path / "matrix.json"
    write_matrix(
        matrix,
        [
            {
                "scope": "shared",
                "form": "plain",
                "shape": "value",
                "direct": True,
                "lifted": False,
                "top_level": True,
                "reason": "-",
            }
        ],
    )
    rows = read_matrix(matrix)
    assert rows["shared/plain/value"]["direct"] is True
    assert rows["shared/plain/value"]["reason"] == "-"


def test_duplicate_matrix_rows_are_rejected(tmp_path: Path) -> None:
    matrix = tmp_path / "matrix.json"
    row = {
        "scope": "shared",
        "form": "plain",
        "shape": "value",
        "direct": True,
        "lifted": False,
        "top_level": True,
        "reason": "-",
    }
    write_matrix(matrix, [row, row])
    with pytest.raises(ValueError):
        read_matrix(matrix)


def test_behavior_rows_are_part_of_the_outcome(tmp_path: Path) -> None:
    matrix = tmp_path / "matrix.json"
    write_matrix(
        matrix,
        [
            {
                "scope": "shared",
                "form": "plain",
                "shape": "value",
                "direct": True,
                "lifted": False,
                "top_level": True,
                "reason": "-",
            }
        ],
    )
    outcomes = build_outcomes(matrix)
    assert outcomes["matrix"]["shared/plain/value"]["top_level"] is True
    assert outcomes["behavior"]
    first = next(iter(outcomes["behavior"].values()))
    assert set(first) == {
        "request",
        "relation",
        "constructions",
        "copies",
        "moves",
        "destructions",
    }


def test_compare_reports_added_removed_and_changed_rows() -> None:
    baseline = {
        "matrix": {
            "shared/plain/value": {
                "direct": True,
                "lifted": False,
                "top_level": True,
                "reason": "-",
            },
            "shared/plain/old": {
                "direct": False,
                "lifted": False,
                "top_level": False,
                "reason": "old",
            },
        },
        "behavior": {},
    }
    current = {
        "matrix": {
            "shared/plain/value": {
                "direct": True,
                "lifted": False,
                "top_level": False,
                "reason": "-",
            },
            "shared/plain/new": {
                "direct": True,
                "lifted": False,
                "top_level": True,
                "reason": "-",
            },
        },
        "behavior": {},
    }
    differences = compare(baseline, current)
    assert "+ matrix shared/plain/new" in differences
    assert "- matrix shared/plain/old" in differences
    assert any(
        difference.startswith("~ matrix shared/plain/value")
        for difference in differences
    )


def test_identical_outcomes_have_no_differences() -> None:
    baseline = {
        "matrix": {
            "shared/plain/value": {
                "direct": True,
                "lifted": False,
                "top_level": True,
                "reason": "-",
            }
        },
        "behavior": {},
    }
    assert compare(baseline, baseline) == []


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
