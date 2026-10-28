#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Derived storage and binding capabilities.

A storage slot is `(scope, storage path)`. The binding kinds a storage exposes
are derived from the scope capabilities and the storage form -- never from a
per-form allowlist. Tags, keys, and interface leaves live on the request side;
the storage only says which binding kinds it can expose.
"""

from __future__ import annotations

from spec.grammar.bindings import Binding
from spec.grammar.catalog import SCOPE_BY_NAME, Scope, StorageForm
from spec.grammar.storage import STORAGE_PATHS, storage_path


def _has_sole_owner(form: StorageForm) -> bool:
    wrapper = form.wrapper
    return wrapper is not None and wrapper.has("sole_owner")


def _has_sharing(form: StorageForm) -> bool:
    wrapper = form.wrapper
    return wrapper is not None and wrapper.has("sharing")


def exposed_binding_kinds(scope: Scope, form: StorageForm) -> frozenset[str]:
    """The binding kinds this storage slot exposes.

    This is the policy table, expressed over scope and form facts:

    - concrete is always available;
    - annotated needs a stable, non-consuming, non-cyclical scope and a form
      that yields a leaf value/reference/pointer, not a sole-owner handle;
    - interface needs a value or handle form and a non-cyclical scope;
    - keyed is registration-level, so every non-cyclical scope can key a form.
    """
    kinds = {"concrete"}

    if (
        scope.stable
        and not scope.consumes
        and not scope.allows_cycles
        and form.kind in {"value", "pointer", "optional", "handle"}
        and not _has_sole_owner(form)
    ):
        kinds.add("annotated")

    if not scope.allows_cycles and form.kind in {"value", "handle"}:
        if form.kind == "value" or _has_sharing(form) or scope.consumes:
            kinds.add("interface")

    if not scope.allows_cycles:
        kinds.add("keyed")

    return frozenset(kinds)


def storage_form(storage_name: str) -> StorageForm | None:
    path = next(
        (item for item in STORAGE_PATHS if item.name == storage_name),
        None,
    )
    return path.form if path is not None else None


def binding_supported(scope: str, storage_name: str, binding: Binding) -> bool:
    path = storage_path(scope, storage_name)
    if path is None:
        return False
    kinds = exposed_binding_kinds(path.scope, path.form)
    if binding.kind == "annotated_interface":
        return {"annotated", "interface"} <= kinds
    return binding.kind in kinds


def binding_gap(
    scope: str, storage_name: str, binding: Binding
) -> str | None:
    if binding_supported(scope, storage_name, binding):
        return None
    path = storage_path(scope, storage_name)
    if path is None:
        return f"{storage_name} has no storage form"
    return f"{scope}/{path.form.name} does not expose the {binding.kind} binding"
