#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.emit.families.indexed import SHARDS, render


def test_indexed_shards_are_hierarchical() -> None:
    assert {shard.key for shard in SHARDS} == {"size_t", "int", "string"}
    for shard in SHARDS:
        assert shard.path == (
            f"resolution/runtime/indexed/global/{shard.key}/resolve.cpp"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, resolve)" in content
        assert shard.traits in content


if __name__ == "__main__":
    raise SystemExit(0)
