#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.collection import build_shards, render

CASES = {
    "interface_vector",
    "keyed_vector",
    "construct_vector",
    "construct_map",
    "keyed_construct_vector",
}


def test_collection_shards_are_hierarchical() -> None:
    shards = build_shards()
    assert {shard.mode for shard in shards} == {"static", "mixed"}
    for shard in shards:
        assert shard.path == f"resolution/{shard.mode}/global/collection.cpp"
        assert {case.name for case in shard.cases} == CASES
        content = render(shard)
        assert content == render(shard)
        for case in shard.cases:
            assert f"TEST({shard.suite}, {case.name})" in content
            assert f'RecordProperty("path", "{shard.case_path}/{case.name}")' in content


def test_mixed_shards_register_at_runtime() -> None:
    for shard in build_shards():
        for case in shard.cases:
            if shard.mode == "mixed":
                assert "container.register_type<" in case.setup
            else:
                assert not case.setup
