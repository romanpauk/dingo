#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Scope/form exposure facts shared by the route model and generators.

The catalog owns the scope policies and the storage forms. This module turns
those facts into the access bits a storage form publishes, so the route ledger, route
rules, and generated storage probes all read the same answer.
"""

from __future__ import annotations

VALUE = 1
LVALUE = 2
RVALUE = 4
POINTER = 8

# Storage forms the library declares but does not implement. The raw dynamic
# array `T[]` has no element count, so no scope can size, construct, or destroy
# it. It stays in the catalog so the form enum and the model stay in sync, but
# no scope exposes it.
UNAVAILABLE_FORMS = frozenset({"array"})


def exposure(scope, form) -> int:
    """Return the exposure of one scope/form pair from scope capabilities.

    Borrowing scopes publish value, lvalue, and pointer access. A consuming
    scope publishes its consumed value plus the leaf lvalue or pointer it can
    still lend. A cyclical scope publishes the leaf and sharing handles; it
    does not store sole-owner or optional forms. Raw arrays are never exposed,
    and the cyclical shared-array handle remains an explicit unavailable form.
    """
    if form.name in UNAVAILABLE_FORMS:
        return 0
    if scope.consumes:
        if form.kind == "value":
            return LVALUE | RVALUE
        if form.kind in {"pointer", "array"}:
            return RVALUE | POINTER
        return RVALUE
    if scope.allows_cycles:
        if form.kind in {"value", "variant", "pointer", "array"}:
            return LVALUE | POINTER
        if form.sharing:
            return VALUE | LVALUE | POINTER
        return 0
    return VALUE | LVALUE | POINTER
