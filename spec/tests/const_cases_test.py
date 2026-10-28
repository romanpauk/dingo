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

from spec.emit.families.const import (
    build_rows,
    build_shards,
    render,
    validate,
)
from spec.emit.common.validation import validate_catalog
from spec.grammar.traits import MOVE_ONLY, TRAITS_BY_NAME


def test_const_catalog_is_non_empty_and_unique() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row["name"] for row in rows}) == len(rows)


def test_const_rows_resolve_const_requests() -> None:
    for row in build_rows():
        assert "tracked" in str(row["request"])
        assert "storage<" in str(row["registration"])


def test_const_rows_are_classified_supported() -> None:
    from spec.grammar.bindings import CONCRETE
    from spec.semantics.support import classify

    for row in build_rows():
        scope, name, request, traits = row["coverage"]
        assert (
            classify(
                scope,
                name,
                CONCRETE,
                request,
                TRAITS_BY_NAME[traits],
            ).outcome
            == "supported"
        )


def test_variant_rows_keep_the_fixed_alternative() -> None:
    rows = [row for row in build_rows() if "variant" in str(row["storage"])]
    assert rows
    for row in rows:
        assert "std::variant<const tracked, long>" in str(row["request"])
        assert "tracked, tracked" not in str(row["request"])


def test_const_move_only_rows_use_the_move_only_leaf() -> None:
    rows = build_rows(MOVE_ONLY)
    assert rows
    assert len(rows) < len(build_rows())
    for row in rows:
        assert "move_only_tracked" in str(row["request"])
        assert "move_only_tracked" in str(row["registration"])
    content = "".join(render(shard) for shard in build_shards() if shard.traits == MOVE_ONLY.name)
    assert "struct move_only_tracked" in content


def test_const_render_is_deterministic(tmp_path: Path) -> None:
    for shard in build_shards()[:4]:
        content = render(shard)
        assert content == render(shard)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[2] / "include")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
