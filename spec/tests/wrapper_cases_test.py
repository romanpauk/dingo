#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.gen.resolution.wrapper import build_shards, render


def test_wrapper_cases_use_model_owned_wrappers() -> None:
    shards = build_shards()
    assert {shard.name for shard in shards} == {
        "shared_wrapper",
        "unique_wrapper",
        "optional_wrapper",
        "variant_wrapper",
    }
    for shard in shards:
        assert shard.path == f"resolution/runtime/global/wrapper/{shard.name}.cpp"
        content = render(shard)
        assert content == render(shard)
        assert "support/custom_wrappers.h" in content
        assert "matrix/" not in content
        assert f"TEST({shard.suite}, {shard.name})" in content
