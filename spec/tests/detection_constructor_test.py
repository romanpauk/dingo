#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from spec.emit.detection_constructor import (
    BACKENDS,
    MODES,
    SHAPES,
    build_shards,
    render,
)


def test_detection_is_backend_by_mode_by_shape() -> None:
    shards = build_shards()
    public_only = {shape for shape in SHAPES if shape.public_only}
    assert len(shards) == len(BACKENDS) * len(MODES) * len(SHAPES) - len(
        MODES
    ) * len(public_only)
    assert len({shard.path for shard in shards}) == len(shards)
    for shard in shards:
        assert shard.path == (
            f"classification/constructor/{shard.backend}/{shard.mode}/"
            f"{shard.shape}.cpp"
        )
        assert shard.case_path == (
            f"classification/constructor/{shard.backend}/{shard.mode}/"
            f"{shard.shape}"
        )


def test_signature_mode_asserts_the_recovered_list() -> None:
    for shard in build_shards():
        content = render(shard)
        assert content == render(shard)
        assert f"TEST({shard.suite}, {shard.shape})" in content
        assert f"== {shard.arity}" in content
        if shard.mode == "signature" and shard.arguments:
            assert "std::is_same_v<typename detection::arguments" in content
            assert shard.arguments in content
        else:
            assert "std::is_same_v<typename detection::arguments" not in content


def test_msvc_backend_runs_on_every_platform() -> None:
    for shard in build_shards():
        content = render(shard)
        if shard.backend == "msvc":
            assert "constructor_detection_msvc" in content
            assert "constructor_detection_msvc.hpp" in content
            assert "GTEST_SKIP" not in content
        else:
            assert "constructor_detection_msvc<" not in content

    shapes = {shard.shape for shard in build_shards()}
    assert "annotated_constructor" in shapes
    assert not [
        shard
        for shard in build_shards()
        if shard.shape == "annotated_constructor" and shard.backend == "msvc"
    ]
