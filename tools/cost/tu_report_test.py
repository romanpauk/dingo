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

from tu_report import rewrite_output, select, summarize


def test_select_keeps_matching_sources() -> None:
    entries = [
        {"file": "/build/generated/matrix/array.cpp"},
        {"file": "/build/generated/model/route_probes.cpp"},
        {"file": "/test/unit/container.cpp"},
    ]
    selected = select(entries, ["generated/matrix"])
    assert [entry["file"] for entry in selected] == [
        "/build/generated/matrix/array.cpp"
    ]


def test_select_without_filters_keeps_everything() -> None:
    entries = [{"file": "a.cpp"}, {"file": "b.cpp"}]
    assert select(entries, []) == entries


def test_rewrite_output_redirects_the_object(tmp_path: Path) -> None:
    command = "c++ -I include -c source.cpp -o CMakeFiles/x.dir/source.cpp.o"
    assert rewrite_output(command, tmp_path / "unit.o") == [
        "c++",
        "-I",
        "include",
        "-c",
        "source.cpp",
        "-o",
        str(tmp_path / "unit.o"),
    ]


def test_rewrite_output_appends_a_missing_output(tmp_path: Path) -> None:
    assert rewrite_output("c++ -c source.cpp", tmp_path / "unit.o") == [
        "c++",
        "-c",
        "source.cpp",
        "-o",
        str(tmp_path / "unit.o"),
    ]


def test_summarize_reports_total_and_extremes() -> None:
    rows = [
        {"seconds_min": 1.0, "object_size": 100},
        {"seconds_min": 3.0, "object_size": 300},
        {"seconds_min": 2.0, "object_size": 200},
    ]
    assert summarize(rows) == {
        "translation_units": 3,
        "total_seconds": 6.0,
        "median_seconds": 2.0,
        "max_seconds": 3.0,
        "total_object_size": 600,
    }


def test_summarize_handles_an_empty_report() -> None:
    assert summarize([]) == {
        "translation_units": 0,
        "total_seconds": 0.0,
        "median_seconds": 0.0,
        "max_seconds": 0.0,
        "total_object_size": 0,
    }


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
