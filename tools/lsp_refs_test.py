#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from pathlib import Path

from tools.lsp_refs import Symbol, flatten, path_of, render, uri


def test_uri_round_trips_through_path_of(tmp_path: Path) -> None:
    path = tmp_path / "a b.h"
    assert uri(path).startswith("file://")
    assert path_of(uri(path)) == path.resolve()


def test_flatten_keeps_names_and_selection_ranges(tmp_path: Path) -> None:
    file = (tmp_path / "sample.h").resolve()
    payload = [
        {
            "name": "dingo",
            "kind": 3,
            "range": {"start": {"line": 0, "character": 0}, "end": {"line": 40, "character": 0}},
            "selectionRange": {"start": {"line": 0, "character": 11}, "end": {"line": 0, "character": 16}},
            "children": [
                {
                    "name": "sample_type",
                    "kind": 23,
                    "range": {"start": {"line": 2, "character": 0}, "end": {"line": 4, "character": 2}},
                    "selectionRange": {"start": {"line": 2, "character": 7}, "end": {"line": 2, "character": 18}},
                }
            ],
        }
    ]
    rows: list[Symbol] = []
    flatten(file, payload, rows)
    # The namespace is a container, not a declaration.
    assert [row.name for row in rows] == ["sample_type"]
    assert rows[0].select == (2, 7)
    assert rows[0].base_name == "sample_type"


def test_render_lists_the_names() -> None:
    text = render(["include/a.h:dead_type"], [])
    assert "dead_type" in text
