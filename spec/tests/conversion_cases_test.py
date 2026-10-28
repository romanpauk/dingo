#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.gen.resolution.conversion import build_shards, render


def test_conversion_cases_cover_both_halves() -> None:
    shards = build_shards()
    assert {shard.name for shard in shards} == {
        "const_reference",
        "volatile_reference",
        "nested_pointer",
        "const_value_from_move_only",
        "copy_only_alternative",
        "copy_only_value",
        "front_end_parity",
    }
    for shard in shards:
        assert shard.path == (
            f"resolution/runtime/global/conversion/{shard.name}.cpp"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.name})" in content
        assert f'RecordProperty("path", "{shard.case_path}")' in content


def test_rejections_assert_the_exception_type() -> None:
    for shard in build_shards():
        content = render(shard)
        if shard.name in {"const_reference", "volatile_reference", "nested_pointer"}:
            assert "type_not_convertible_exception" in content
            assert "EXPECT_THROW" in content


def test_parity_runs_every_front_end() -> None:
    shard = next(s for s in build_shards() if s.name == "front_end_parity")
    content = render(shard)
    for spelling in (
        "dingo::runtime_container<>",
        "dingo::container<>",
        "dingo::static_container<static_source>",
    ):
        assert spelling in content
