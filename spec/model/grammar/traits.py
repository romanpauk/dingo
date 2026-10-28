#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Leaf copy and move traits.

A path describes a type shape; the leaf's special members decide whether a
route that would copy the leaf is available. The traits are a separate
dimension of the domain: every storage form and request shape is classified
once per trait.

The leaf spelling is the C++ fixture the generated probes use. The traits
themselves do not name a wrapper or a scope; they only say whether the leaf can
be copied and whether it can be moved.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class LeafTraits:
    name: str
    cpp: str
    copyable: bool
    movable: bool

    def __str__(self) -> str:
        return self.name


COPYABLE = LeafTraits("copyable", "tracked", copyable=True, movable=True)
MOVE_ONLY = LeafTraits("move_only", "move_only_tracked", copyable=False, movable=True)

TRAITS = (COPYABLE, MOVE_ONLY)
TRAITS_BY_NAME = {traits.name: traits for traits in TRAITS}

# The default keeps the existing path domain single-trait. The trait table
# classifies the same domain once per entry of TRAITS.
DEFAULT_TRAITS = COPYABLE
