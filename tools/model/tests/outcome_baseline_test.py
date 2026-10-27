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

from tools.model.report.baseline import build_outcomes, compare, read_behavior, read_matrix


def write_matrix(path: Path, rows: list[dict[str, object]]) -> None:
    path.write_text(
        "\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8"
    )


def write_behavior(path: Path, rows: list[dict[str, object]]) -> None:
    path.write_text(
        "\n".join(json.dumps(row) for row in rows) + "\n", encoding="utf-8"
    )


def test_matrix_rows_are_keyed_by_form_and_shape(tmp_path: Path) -> None:
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


def test_behavior_rows_are_actual_runtime_outcomes(tmp_path: Path) -> None:
    matrix = tmp_path / "matrix.json"
    behavior = tmp_path / "behavior.json"
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
    write_behavior(
        behavior,
        [
            {
                "name": "shared_plain_runtime",
                "scope": "shared",
                "form": "plain",
                "container": "runtime",
                "request": "tracked",
                "result": "ok",
                "constructions": 1,
                "copies": 2,
                "moves": 0,
                "destructions": 3,
            }
        ],
    )
    outcomes = build_outcomes(matrix, behavior)
    assert outcomes["matrix"]["shared/plain/value"]["top_level"] is True
    row = outcomes["behavior"]["shared_plain_runtime"]
    assert row["result"] == "ok"
    assert row["constructions"] == 1
    assert row["copies"] == 2
    assert row["destructions"] == 3


def test_behavior_report_requires_a_result(tmp_path: Path) -> None:
    behavior = tmp_path / "behavior.json"
    write_behavior(
        behavior,
        [
            {
                "name": "row",
                "scope": "shared",
                "form": "plain",
                "container": "runtime",
                "request": "tracked",
                "result": "ok",
                "constructions": 0,
                "copies": 0,
                "moves": 0,
                "destructions": 0,
            }
        ],
    )
    assert read_behavior(behavior)["row"]["result"] == "ok"


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
