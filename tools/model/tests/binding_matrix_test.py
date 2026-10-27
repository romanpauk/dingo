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

from tools.model.emit.binding_matrix import (
    build_rows,
    build_shards,
    manifest,
    render,
    validate,
)
from tools.model.emit.common.validation import validate_catalog
from tools.model.grammar.bindings import binding_for
from tools.model.grammar.traits import TRAITS_BY_NAME


def test_binding_matrix_is_non_empty_and_unique() -> None:
    validate()
    rows = build_rows()
    assert rows
    keys = {
        (row["scope"], row["storage"], row["traits"], row["binding"], row["request"])
        for row in rows
    }
    assert len(keys) == len(rows)


def test_binding_matrix_rows_are_supported() -> None:
    from tools.model.semantics.support import classify

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


def test_binding_shards_follow_the_registration_hierarchy() -> None:
    shards = build_shards()
    assert shards
    assert len({shard.path for shard in shards}) == len(shards)
    for shard in shards:
        assert shard.path == (
            f"registration/static_asserts/{shard.scope}/{shard.storage}/binding.cpp"
        )
        assert shard.cases
        for case in shard.cases:
            assert case.path.startswith(
                f"registration/static_asserts/{shard.scope}/{shard.storage}/binding/"
            )


def test_binding_matrix_render_is_deterministic() -> None:
    for shard in build_shards()[:4]:
        content = render(shard)
        assert content == render(shard)
        for case in shard.cases:
            assert f"TEST({shard.suite}, {case.name})" in content
            assert f'RecordProperty("path", "{case.path}")' in content


def test_binding_manifest_round_trips() -> None:
    shards = build_shards()
    payload = json.loads(manifest(shards))
    assert len(payload["shards"]) == len(shards)
    for shard, entry in zip(shards, payload["shards"]):
        assert entry["path"] == shard.path
        assert len(entry["cases"]) == len(shard.cases)


def test_library_catalog_matches_model() -> None:
    validate_catalog(Path(__file__).resolve().parents[3] / "include")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
