#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Supported request shapes outside the bounded base shape catalog."""

from __future__ import annotations

from functools import lru_cache

from tools.model.domain.cases import iter_cases, request_forms
from tools.model.grammar.shapes import SHAPES, Shape
from tools.model.semantics.support import classify, to_shape


@lru_cache(maxsize=None)
def supported_shapes() -> tuple[Shape, ...]:
    """The unique request shapes that at least one classified path supports."""
    shapes = {}
    for case in iter_cases(request_forms()):
        if (
            classify(
                case.scope,
                case.storage_name,
                case.binding,
                case.request,
            ).outcome
            != "supported"
        ):
            continue
        shape = to_shape(case.request)
        if shape is not None:
            shapes[shape.name] = shape
    return tuple(sorted(shapes.values(), key=lambda shape: shape.name))


@lru_cache(maxsize=None)
def extended_shapes() -> tuple[Shape, ...]:
    """Supported shapes outside the bounded base depth-2 catalog."""
    current = {shape.name for shape in SHAPES}
    return tuple(
        shape for shape in supported_shapes() if shape.name not in current
    )
