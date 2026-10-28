#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""Validate the model catalogs against the library declarations."""

from __future__ import annotations

import re
from pathlib import Path

from spec.model.grammar.catalog import FORMS, SCOPES
from spec.model.semantics.forms import exposure


def validate() -> None:
    scope_names = [scope.name for scope in SCOPES]
    form_names = [form.name for form in FORMS]
    if len(scope_names) != len(set(scope_names)):
        raise ValueError("duplicate scope name")
    if len(form_names) != len(set(form_names)):
        raise ValueError("duplicate form name")
    if not scope_names or not form_names:
        raise ValueError("scope and form catalogs must not be empty")
    for scope in SCOPES:
        for form in FORMS:
            exposure(scope, form)


def _strip_comments(text: str) -> str:
    return re.sub(r"//[^\n]*", "", text)


def catalog_names(include_directory: Path) -> tuple[set[str], set[str]]:
    """Read the library's built-in scope and form names.

    The model is the source of coverage, but the library owns the declaration.
    Comparing them here makes a new built-in scope or form fail the check when
    the model has not been extended, instead of silently leaving the new form
    untested.
    """
    storage_root = include_directory / "dingo" / "storage"
    policy_header = storage_root / "storage_scope_policy.h"
    policy_text = _strip_comments(policy_header.read_text(encoding="utf-8"))
    form_match = re.search(
        r"enum\s+class\s+storage_form\s*\{(.*?)\};",
        policy_text,
        re.DOTALL,
    )
    if form_match is None:
        raise ValueError(f"storage_form enum not found in {policy_header}")
    library_forms = {
        name.strip()
        for name in form_match.group(1).split(",")
        if name.strip() and name.strip() != "unknown"
    }

    library_scopes = set()
    for header in sorted(storage_root.glob("*.h")):
        text = _strip_comments(header.read_text(encoding="utf-8"))
        for match in re.finditer(
            r"storage_scope_policy\s*<\s*([A-Za-z_]\w*)\s*>", text
        ):
            name = match.group(1)
            if name not in {"StorageTag", "Scope"}:
                library_scopes.add(name)

    return library_scopes, library_forms


def validate_catalog(include_directory: Path) -> None:
    validate()
    library_scopes, library_forms = catalog_names(include_directory)
    model_scopes = {scope.name for scope in SCOPES}
    model_forms = {form.name for form in FORMS}
    if library_scopes != model_scopes:
        raise ValueError(
            "scope catalog is out of date: "
            f"missing {sorted(library_scopes - model_scopes)}, "
            f"stale {sorted(model_scopes - library_scopes)}"
        )
    if library_forms != model_forms:
        raise ValueError(
            "form catalog is out of date: "
            f"missing {sorted(library_forms - model_forms)}, "
            f"stale {sorted(model_forms - library_forms)}"
        )
