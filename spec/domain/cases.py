#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Enumerate the bounded path domain.

The types come from `grammar/enumerate.py`; this module pairs the request projection and
the storage projection into cases. Support classification prunes the resulting
tree in `semantics/support.py`.
"""

from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache

from spec.grammar.bindings import Binding
from spec.domain.requests import projected_requests, request_forms as derived_request_forms
from spec.grammar.catalog import SCOPES
from spec.grammar.enumerate import (
    EXTENTS,
    QUALIFIER_DEPTH,
    annotated_forms,
    interface_forms,
    keyed_forms,
    plain_forms,
    request_types,
    variant_forms,
    variant_pool,
    wrapped_forms,
)
from spec.grammar.storage import STORAGE_PATHS, StoragePath
from spec.grammar.expr import Expr
from spec.grammar.traits import DEFAULT_TRAITS, LeafTraits


def request_forms(
    depth: int = QUALIFIER_DEPTH,
    extents: tuple[int, ...] = EXTENTS,
) -> tuple[Expr, ...]:
    """The union of the per-storage request projections."""
    return derived_request_forms()


@dataclass(frozen=True, slots=True)
class Case:
    storage_path: StoragePath
    binding: Binding
    request: Expr
    traits: LeafTraits = DEFAULT_TRAITS

    @property
    def scope(self) -> str:
        return self.storage_path.scope.name

    @property
    def storage_name(self) -> str:
        return self.storage_path.name

    @property
    def storage(self) -> Expr:
        return self.storage_path.expr


@lru_cache(maxsize=None)
def iter_cases(
    request: tuple[Expr, ...] | None = None,
    traits: LeafTraits = DEFAULT_TRAITS,
) -> tuple[Case, ...]:
    cases = []
    for storage in STORAGE_PATHS:
        for binding, expr in projected_requests(storage):
            cases.append(Case(storage, binding, expr, traits))
    return tuple(cases)


@lru_cache(maxsize=None)
def counts() -> dict[str, int]:
    plain = plain_forms()
    wrapped = wrapped_forms(plain)
    variants = variant_forms(variant_pool(wrapped))
    base = set(plain) | set(wrapped) | set(variants)
    annotated = annotated_forms(tuple(base))
    keyed_forms_ = keyed_forms(tuple(base))
    interface_forms_ = interface_forms(tuple(base))
    requests = request_forms()
    slots = len(STORAGE_PATHS)
    cases = iter_cases(requests)
    return {
        "plain": len(plain),
        "wrapped": len(wrapped),
        "variants": len(variants),
        "annotated": len(annotated),
        "keyed": len(keyed_forms_),
        "interface": len(interface_forms_),
        "requests": len(requests),
        "storages": len(STORAGE_PATHS),
        "bindings": 5,
        "scopes": len(SCOPES),
        "slots": slots,
        "cases": len(cases),
    }


if __name__ == "__main__":
    for key, value in counts().items():
        print(f"{key}: {value}")
