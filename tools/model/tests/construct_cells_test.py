#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.construct import GAPS, build_shards, render
from tools.model.grammar.traits import COPYABLE, MOVE_ONLY


def test_construct_shapes_are_operator_classes() -> None:
    shards = build_shards()
    assert shards
    assert len({shard.path for shard in shards}) == len(shards)
    assert {shard.traits for shard in shards} == {
        COPYABLE.name,
        MOVE_ONLY.name,
    }
    for shard in shards:
        assert shard.path == (
            f"resolution/runtime/global/construct/{shard.shape}/"
            f"{shard.traits}.cpp"
        )
        assert shard.case_path == (
            f"resolution/runtime/global/construct/{shard.shape}/{shard.traits}"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.shape})" in content
        assert shard.setup or shard.shape == "default"
        assert f'RecordProperty("path", "{shard.case_path}")' in content


def test_construct_gaps_are_declared_with_a_reason() -> None:
    assert set(GAPS) == {"optional_parameter"}
    assert all(reason for reason in GAPS.values())


if __name__ == "__main__":
    raise SystemExit(0)
