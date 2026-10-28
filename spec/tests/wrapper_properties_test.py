#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from pathlib import Path

import pytest

from spec.gen.classification.wrapper import (
    build_rows,
    build_shards,
    render,
    validate,
)
from spec.model.grammar.shapes import CUSTOM_WRAPPERS, WRAPPERS


def test_property_catalog_is_complete() -> None:
    validate()
    rows = build_rows()
    assert rows
    assert len({row.name for row in rows}) == len(rows)
    builtins = {row.name for row in rows if row.name.startswith("property_")}
    assert {
        "property_pointer",
        "property_optional",
        "property_unique_ptr",
        "property_shared_ptr",
        "property_unique_array",
        "property_shared_array",
        "property_variant",
    } <= builtins


def test_custom_wrappers_are_in_the_generated_catalog() -> None:
    names = {row.name for row in build_rows()}
    assert {f"property_{wrapper.name}" for wrapper in CUSTOM_WRAPPERS} <= names


def test_every_declared_wrapper_has_a_row() -> None:
    generated = {row.name for row in build_rows()}
    for wrapper in (*WRAPPERS, *CUSTOM_WRAPPERS):
        if wrapper.kind in {
            "value",
            "handle",
            "array_handle",
            "variant",
            "pointer",
        }:
            assert f"property_{wrapper.name}" in generated


def test_properties_are_consistent() -> None:
    for row in build_rows():
        if row.can_take:
            assert row.is_owning
        if row.can_copy_out:
            assert row.is_borrowable


def test_render_is_stable(tmp_path: Path) -> None:
    shards = build_shards()
    assert len(shards) == len(build_rows())
    for shard in shards:
        content = render(shard)
        assert content == render(shard)
        assert shard.row.cpp_type in content
        assert f'RecordProperty("path", "{shard.case_path}")' in content


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
