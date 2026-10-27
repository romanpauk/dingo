#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.configuration import SHARDS, render


def test_configuration_shards_are_one_file_per_trait() -> None:
    assert {shard.configuration for shard in SHARDS} == {"allocator", "rtti"}
    for shard in SHARDS:
        assert shard.path == (
            f"resolution/runtime/{shard.configuration}/global/resolve.cpp"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, resolve)" in content
        assert f'RecordProperty("path", "{shard.case_path}")' in content
