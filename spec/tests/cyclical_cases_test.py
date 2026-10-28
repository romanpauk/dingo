#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.gen.resolution.cyclical import build_shards, render


def test_cyclical_shards_cover_modes_and_storage() -> None:
    shards = build_shards()
    assert {shard.mode for shard in shards} == {"runtime", "static", "mixed"}
    for shard in shards:
        assert shard.path == f"resolution/{shard.mode}/global/cyclical.cpp"
        content = render(shard)
        assert content == render(shard)
        for case in shard.cases:
            assert f"TEST({shard.suite}, {case.name})" in content
            assert f'RecordProperty("path", "{shard.case_path}/{case.name}")' in content
            assert "EXPECT_EQ(a.peer, &b);" in content
            assert "EXPECT_EQ(b.peer, &a);" in content


def test_every_case_stores_both_nodes() -> None:
    for shard in build_shards():
        for case in shard.cases:
            if shard.mode == "static":
                assert "dingo::static_container<source>" in case.container
            else:
                assert "dingo::container" in case.container
