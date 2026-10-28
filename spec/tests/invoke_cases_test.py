#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.emit.families.invoke import GAPS, build_shards, render
from spec.grammar.traits import COPYABLE, MOVE_ONLY


def test_invoke_shapes_are_operator_classes() -> None:
    shards = build_shards()
    assert shards
    assert len({shard.path for shard in shards}) == len(shards)
    assert {shard.traits for shard in shards} == {
        COPYABLE.name,
        MOVE_ONLY.name,
    }
    for shard in shards:
        assert shard.path == (
            f"resolution/runtime/global/invoke/{shard.shape}/{shard.traits}.cpp"
        )
        assert shard.case_path == (
            f"resolution/runtime/global/invoke/{shard.shape}/{shard.traits}"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.shape})" in content
        assert f'RecordProperty("path", "{shard.case_path}")' in content


def test_invoke_covers_the_callable_forms() -> None:
    shapes = {shard.shape for shard in build_shards()}
    assert {
        "no_arguments",
        "lvalue_reference",
        "const_lvalue_reference",
        "value",
        "rvalue_reference",
        "pointer",
        "const_pointer",
        "shared_handle",
        "two_arguments",
        "void_return",
        "std_function",
        "function_pointer",
        "functor",
        "explicit_signature",
        "overloaded_signature",
        "move_only_return",
        "reference_return",
        "missing_dependency",
    } == shapes


def test_invoke_gaps_are_declared_with_a_reason() -> None:
    assert set(GAPS) == {"ambiguous_overload"}
    assert all(reason for reason in GAPS.values())
