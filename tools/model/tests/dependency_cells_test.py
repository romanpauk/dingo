#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.dependencies import GAPS, build_shards, render


def test_dependency_cases_are_the_two_declarations() -> None:
    shards = build_shards()
    assert {shard.name for shard in shards} == {
        "runtime_dependencies",
        "static_dependencies",
    }
    for shard in shards:
        assert shard.path == (
            f"resolution/runtime/global/dependencies/{shard.name}.cpp"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.name})" in content
        assert "EXPECT_EQ(container.resolve<selected_arguments &>().value, 17);" in content


def test_construct_gap_is_declared() -> None:
    assert set(GAPS) == {"construct_with_registration_dependencies"}
    assert all(reason for reason in GAPS.values())
