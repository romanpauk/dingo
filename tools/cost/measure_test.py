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

from measure import compare_metric, trace_summary


def test_compare_metric_accepts_equal_values() -> None:
    problems: list[str] = []
    compare_metric("text", 100, 100, 0.0, problems)
    assert problems == []


def test_compare_metric_rejects_growth() -> None:
    problems: list[str] = []
    compare_metric("text", 100, 101, 0.0, problems)
    assert problems == ["text: 101 > 100 (+0.0%)"]


def test_compare_metric_allows_explicit_tolerance() -> None:
    problems: list[str] = []
    compare_metric("text", 1000, 1005, 0.01, problems)
    assert problems == []


def test_trace_summary_counts_dingo_events(tmp_path: Path) -> None:
    trace = tmp_path / "trace.json"
    trace.write_text(
        json.dumps(
            {
                "traceEvents": [
                    {"name": "dingo::detail::resolve"},
                    {"name": "std::vector"},
                    {"name": "compile", "args": {"detail": "dingo::binding"}},
                ]
            }
        ),
        encoding="utf-8",
    )
    assert trace_summary(trace) == {
        "trace_events": 3,
        "dingo_trace_events": 2,
    }


def test_missing_trace_is_empty(tmp_path: Path) -> None:
    assert trace_summary(tmp_path / "missing.json") == {
        "trace_events": 0,
        "dingo_trace_events": 0,
    }


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
