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

from tools.model.emit.resolve import build_shards, manifest, render
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY


def test_resolve_shards_follow_the_storage_hierarchy() -> None:
    shards = build_shards()
    assert shards
    assert len({shard.path for shard in shards}) == len(shards)
    for shard in shards:
        assert shard.path == (
            f"{shard.container}/{shard.scope}/{shard.storage}.cpp"
        )
        assert shard.suite == (
            f"resolve_{shard.container}_{shard.scope}_{shard.storage}"
        )
        assert shard.cases


def test_resolve_cases_are_unique_and_cover_both_traits() -> None:
    cases = [case for shard in build_shards() for case in shard.cases]
    assert cases
    assert len({(case.suite, case.name) for case in cases}) == len(cases)
    assert len({case.path for case in cases}) == len(cases)
    assert {case.traits for case in cases} == {
        COPYABLE.name,
        MOVE_ONLY.name,
    }
    assert all(case.request for case in cases)
    assert all(case.registration for case in cases)


def test_resolve_render_is_one_test_per_case() -> None:
    for shard in build_shards()[:4]:
        content = render(shard)
        assert content == render(shard)
        for case in shard.cases:
            assert f"TEST({shard.suite}, {case.name})" in content
        assert content.count("GTEST_SKIP()") == len(shard.cases)


def test_resolve_manifest_round_trips() -> None:
    shards = build_shards()
    payload = json.loads(manifest(shards))
    assert len(payload["shards"]) == len(shards)
    for shard, entry in zip(shards, payload["shards"]):
        assert entry["path"] == shard.path
        assert entry["suite"] == shard.suite
        assert entry["storage"] == shard.storage
        assert len(entry["cases"]) == len(shard.cases)
        for case, case_entry in zip(shard.cases, entry["cases"]):
            assert case_entry["name"] == case.name
            assert case_entry["path"] == case.path
            assert case_entry["request"] == case.request


if __name__ == "__main__":
    raise SystemExit(0)
