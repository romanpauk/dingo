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

from tools.model.emit.families.annotated import (
    build_rows,
    build_shards,
    render,
    validate,
)
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar.traits import MOVE_ONLY


def test_annotated_catalog_is_non_empty_and_unique() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)


def test_annotated_rows_use_the_bound_request() -> None:
    for row in build_rows():
        assert "dingo::annotated<" in str(row["request"])
        assert "interfaces<dingo::annotated<tracked, tag_a>>" in str(
            row["registration"]
        )
        assert row["relation"]


def test_annotated_move_only_rows_use_the_move_only_leaf() -> None:
    rows = build_rows(MOVE_ONLY)
    assert rows
    assert len(rows) < len(build_rows())
    for row in rows:
        assert "move_only_tracked" in str(row["request"])
        assert (
            "interfaces<dingo::annotated<move_only_tracked, tag_a>>"
            in str(row["registration"])
        )
    content = "".join(render(shard) for shard in build_shards() if shard.traits == MOVE_ONLY.name)
    assert "struct move_only_tracked" in content


def test_annotated_render_is_deterministic(tmp_path: Path) -> None:
    for shard in build_shards()[:4]:
        content = render(shard)
        assert content == render(shard)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
