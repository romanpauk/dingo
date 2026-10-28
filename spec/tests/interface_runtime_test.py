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

from spec.emit.common.validation import validate_catalog
from spec.emit.families.interface_runtime import (
    build_rows,
    build_shards,
    render,
    validate,
)
from spec.grammar.bindings import binding_for
from spec.grammar.traits import MOVE_ONLY, TRAITS_BY_NAME
from spec.semantics.support import classify


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
    content = "".join(render(shard) for shard in build_shards() if shard.traits == MOVE_ONLY.name)
    assert content


def test_interface_render_is_deterministic(tmp_path: Path) -> None:
    for shard in build_shards()[:4]:
        content = render(shard)
        assert content == render(shard)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[2] / "include")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
