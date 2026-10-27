#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.collection import SHARDS, render


def test_collection_shards_are_hierarchical() -> None:
    assert {shard.kind for shard in SHARDS} == {"interface", "keyed"}
    for shard in SHARDS:
        assert shard.path == (
            f"resolution/static/global/collection/{shard.kind}.cpp"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, resolve)" in content
        assert f"TEST({shard.suite}, construct)" in content


if __name__ == "__main__":
    raise SystemExit(0)
