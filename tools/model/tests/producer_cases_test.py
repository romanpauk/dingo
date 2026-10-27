#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from tools.model.emit.families.producer import CLAIMS, build_shards, render


def test_producer_forms_are_operator_independent() -> None:
    shards = build_shards()
    assert shards
    assert len({shard.path for shard in shards}) == len(shards)
    for shard in shards:
        assert shard.path == (
            f"resolution/runtime/global/producer/{shard.target}.cpp"
        )
        assert shard.cases
        content = render(shard)
        assert content == render(shard)
        for case in shard.cases:
            for claim in CLAIMS:
                assert f"TEST({shard.suite}, {case.kind}_{claim})" in content
                assert (
                    f'RecordProperty("path", "{shard.case_path}/'
                    f'{case.kind}/{claim}")' in content
                )


def test_producer_covers_every_producer_kind() -> None:
    kinds = {
        case.kind for shard in build_shards() for case in shard.cases
    }
    assert {
        "detected",
        "constructor",
        "function",
        "callable",
        "callable_signature",
        "typedef",
        "overloaded_callable",
    } <= kinds
