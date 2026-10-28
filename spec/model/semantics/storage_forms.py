#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The scope/form storage product and its supported subset.

Families iterate the same product: route probes, the type matrix, type
behavior, and behavior all consume every supported pair. Keeping the iteration
here means the "is this storage form publishable" question has one answer.
"""

from __future__ import annotations

from collections.abc import Iterator

from spec.model.grammar.catalog import FORMS, SCOPES, Scope, StorageForm
from spec.model.semantics.routes.ledger import supported_categories


def iter_storage_forms() -> Iterator[tuple[Scope, StorageForm]]:
    for scope in SCOPES:
        for form in FORMS:
            yield scope, form


def iter_supported_forms(
    *, form_major: bool = False
) -> Iterator[tuple[Scope, StorageForm]]:
    if form_major:
        pairs = (
            (scope, form) for form in FORMS for scope in SCOPES
        )
    else:
        pairs = iter_storage_forms()
    for scope, form in pairs:
        if supported_categories(scope.name, form.name):
            yield scope, form
