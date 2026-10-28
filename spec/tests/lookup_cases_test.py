#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.gen.resolution.lookup import build_shards, render


def test_lookup_cases_are_rules_not_forms() -> None:
    shards = build_shards()
    assert {shard.name for shard in shards} == {
        "child_inherits",
        "child_shadows",
        "grandchild",
    }
    for shard in shards:
        assert shard.path == (
            f"resolution/runtime/global/lookup/{shard.name}.cpp"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.name})" in content
        assert f'RecordProperty("path", "{shard.case_path}")' in content
