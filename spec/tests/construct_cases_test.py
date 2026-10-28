#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.emit.families.construct import (
    ARGUMENTS,
    GAPS,
    SHAPES,
    build_shards,
    render,
)


def test_construct_is_argument_type_by_shape() -> None:
    shards = build_shards()
    assert len({shard.path for shard in shards}) == len(shards)
    typed = [shard for shard in shards if shard.argument in
             {argument.name for argument in ARGUMENTS}]
    assert len(typed) == len(ARGUMENTS) * len(SHAPES) - sum(
        1 for key in GAPS if "/" in key
    )
    for shard in typed:
        assert shard.path == (
            f"resolution/runtime/global/construct/{shard.argument}/"
            f"{shard.shape}.cpp"
        )
        assert shard.case_path == (
            f"resolution/runtime/global/construct/{shard.argument}/"
            f"{shard.shape}"
        )
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.shape})" in content
        assert f'RecordProperty("path", "{shard.case_path}")' in content


def test_construct_argument_types_cover_every_dependency_form() -> None:
    names = {argument.name for argument in ARGUMENTS}
    assert {
        "value",
        "value_move_only",
        "lvalue_reference",
        "const_lvalue_reference",
        "rvalue_reference",
        "pointer",
        "const_pointer",
        "shared_handle",
        "shared_handle_reference",
        "shared_handle_const_reference",
        "unique_handle",
        "optional_reference",
        "variant",
        "variant_reference",
        "array_reference",
        "annotated_reference",
    } == names


def test_construct_shapes_cover_detection_forms() -> None:
    assert {shape.name for shape in SHAPES} == {
        "deduced",
        "twice",
        "selected",
        "explicit",
        "highest_arity",
    }


def test_construct_gaps_are_declared_with_a_reason() -> None:
    assert all(reason for reason in GAPS.values())
