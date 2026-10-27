#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import json

from tools.model.emit.detection_constructor import SHAPES, manifest, render


def test_detection_shapes_have_unique_paths() -> None:
    paths = [f"classification/constructor/{shape.name}" for shape in SHAPES]
    assert len(paths) == len(set(paths))
    assert SHAPES


def test_detection_render_is_deterministic() -> None:
    for shape in SHAPES:
        content = render(shape)
        assert content == render(shape)
        assert (
            f'detected constructor arity == {shape.arity}' not in content
        )
        assert f"== {shape.arity}" in content
        assert f'RecordProperty("path", "classification/constructor/{shape.name}")' in content


def test_detection_manifest_round_trips() -> None:
    payload = json.loads(manifest())
    assert len(payload["shapes"]) == len(SHAPES)
    for shape, entry in zip(SHAPES, payload["shapes"]):
        assert entry["name"] == shape.name
        assert entry["arity"] == shape.arity
        assert entry["path"] == f"classification/constructor/{shape.name}"


if __name__ == "__main__":
    raise SystemExit(0)
