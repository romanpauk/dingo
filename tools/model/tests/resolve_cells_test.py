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

from tools.model.emit.resolve import (
    build_shards,
    manifest,
    render,
)
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY


def test_resolve_shards_are_complete_and_unique() -> None:
    shards = build_shards()
    assert shards
    assert len({shard.name for shard in shards}) == len(shards)
    tests = [test for shard in shards for test in shard.tests]
    assert tests
    assert len({test.name for test in tests}) == len(tests)
    calls = [call for test in tests for call in test.calls]
    assert calls
    assert len({call.name for call in calls}) == len(calls)
    assert all(call.expression.startswith("container.template resolve<") for call in calls)


def test_resolve_shards_cover_both_leaf_traits() -> None:
    traits = {
        test.traits
        for shard in build_shards()
        for test in shard.tests
    }
    assert traits == {COPYABLE.name, MOVE_ONLY.name}


def test_resolve_render_is_deterministic(tmp_path: Path) -> None:
    shards = build_shards()
    for shard in shards:
        content = render(shard)
        assert content == render(shard)
        assert f"TEST(resolve_cells, {shard.tests[0].name})" in content
        assert "write_resolve_outcome" in content


def test_resolve_manifest_round_trips(tmp_path: Path) -> None:
    shards = build_shards()
    payload = json.loads(manifest(shards))
    assert len(payload["shards"]) == len(shards)
    for shard, entry in zip(shards, payload["shards"]):
        assert entry["name"] == shard.name
        assert len(entry["tests"]) == len(shard.tests)
        for test, test_entry in zip(shard.tests, entry["tests"]):
            assert test_entry["name"] == test.name
            assert len(test_entry["calls"]) == len(test.calls)


if __name__ == "__main__":
    raise SystemExit(0)
