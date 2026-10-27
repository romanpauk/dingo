#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.factory import SHARDS, render


def test_factory_shards_are_hierarchical() -> None:
    assert SHARDS
    assert {shard.kind for shard in SHARDS} == {"function", "callable"}
    for shard in SHARDS:
        assert shard.path == (
            f"registration/runtime/shared/plain/factory/{shard.kind}.cpp"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.kind})" in content
        assert f'RecordProperty("path", "{shard.case_path}")' in content


if __name__ == "__main__":
    raise SystemExit(0)
