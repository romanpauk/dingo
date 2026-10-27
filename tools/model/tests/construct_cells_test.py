#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.construct import build_shards, render
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY


def test_construct_shards_are_unique_and_cover_both_traits() -> None:
    shards = build_shards()
    assert shards
    assert len({shard.path for shard in shards}) == len(shards)
    assert {shard.traits for shard in shards} == {
        COPYABLE.name,
        MOVE_ONLY.name,
    }
    for shard in shards:
        assert shard.path.endswith(f"construct/{shard.traits}.cpp")
        assert shard.registration
        content = render(shard)
        assert content == render(shard)
        assert f"construct<{shard.constructed}>" in content
        assert f'RecordProperty("path", "resolution/runtime/{shard.scope}/' in content


if __name__ == "__main__":
    raise SystemExit(0)
