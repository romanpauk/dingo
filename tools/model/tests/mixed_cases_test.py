#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.mixed import SHARDS, render


def test_mixed_cases_share_the_container_but_not_the_assertion() -> None:
    assert {shard.name for shard in SHARDS} == {
        "external_dependency",
        "singular_ambiguity",
    }
    for shard in SHARDS:
        assert shard.path == f"resolution/mixed/global/{shard.name}.cpp"
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.name})" in content
        assert 'dingo::container<source>' in content
        if shard.name == "singular_ambiguity":
            assert "EXPECT_THROW" in content
        else:
            assert "EXPECT_EQ" in content
