#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The explicit route ledger for the built-in storage cells.

Every scope, storage form, and route category has exactly one outcome:

- supported: the cell publishes the category;
- unavailable: the scope/form combination intentionally cannot publish it;
- gap: the route shape is not modeled yet, with a tracked gap id.

No row may be absent, and no gap is silent.
"""

from __future__ import annotations

from dataclasses import dataclass

from storage_cells import FORMS, LVALUE, POINTER, RVALUE, SCOPES, VALUE, exposure


CATEGORIES = ("value", "object", "rvalue")
ROUTE_FORM_NAMES = (
    "plain",
    "alternative",
    "pointer",
    "unique_handle",
    "shared_handle",
    "optional",
)
GAP_ARRAY_ROUTES = "array-routes"


@dataclass(frozen=True)
class LedgerRow:
    scope: str
    form: str
    category: str
    outcome: str
    reason: str = ""
    gap: str = ""
    access: str = ""


def _category_outcome(
    scope_name: str, form_name: str, bits: int, category: str
) -> LedgerRow:
    scope = next(scope for scope in SCOPES if scope.name == scope_name)
    form = next(form for form in FORMS if form.name == form_name)
    if bits == 0:
        return LedgerRow(
            scope_name,
            form_name,
            category,
            "unavailable",
            "scope does not publish the form",
        )
    if form_name not in ROUTE_FORM_NAMES:
        return LedgerRow(
            scope_name,
            form_name,
            category,
            "gap",
            "array route shape is not modeled",
            GAP_ARRAY_ROUTES,
        )
    if category == "value":
        supported = bool(bits & VALUE and form.has_value) or bool(bits & RVALUE)
        reason = "scope/form publishes no value route"
    elif category == "object":
        supported = bool(bits & (LVALUE | POINTER))
        reason = "scope/form publishes no object route"
    else:
        supported = bool(bits & RVALUE)
        reason = "scope/form publishes no rvalue route"
    return LedgerRow(
        scope_name,
        form_name,
        category,
        "supported" if supported else "unavailable",
        "" if supported else reason,
        "",
        (
            "consume"
            if supported and category == "value" and scope.consumes
            else "consume"
            if supported and category == "rvalue"
            else "borrow"
            if supported
            else ""
        ),
    )


def build_rows() -> list[LedgerRow]:
    rows = []
    for scope in SCOPES:
        for form in FORMS:
            bits = exposure(scope, form)
            for category in CATEGORIES:
                rows.append(
                    _category_outcome(scope.name, form.name, bits, category)
                )
    return rows


def validate() -> None:
    rows = build_rows()
    expected = len(SCOPES) * len(FORMS) * len(CATEGORIES)
    if len(rows) != expected:
        raise ValueError("route ledger is incomplete")
    keys = {(row.scope, row.form, row.category) for row in rows}
    if len(keys) != len(rows):
        raise ValueError("route ledger contains duplicate rows")
    for row in rows:
        if row.outcome not in {"supported", "unavailable", "gap"}:
            raise ValueError(f"unknown route outcome {row.outcome!r}")
        if row.outcome == "supported" and row.reason:
            raise ValueError(f"supported row has a reason: {row}")
        if row.outcome == "unavailable" and not row.reason:
            raise ValueError(f"unavailable row has no reason: {row}")
        if row.outcome == "gap" and not row.gap:
            raise ValueError(f"gap row has no id: {row}")
        if row.outcome == "supported" and row.access not in {"borrow", "consume"}:
            raise ValueError(f"supported row has no access: {row}")
        if row.outcome != "supported" and row.access:
            raise ValueError(f"non-supported row has an access: {row}")


def supported_categories(scope_name: str, form_name: str) -> set[str]:
    return {
        row.category
        for row in build_rows()
        if row.scope == scope_name
        and row.form == form_name
        and row.outcome == "supported"
    }


def category_access(scope_name: str, form_name: str, category: str) -> str:
    for row in build_rows():
        if (
            row.scope == scope_name
            and row.form == form_name
            and row.category == category
            and row.outcome == "supported"
        ):
            return row.access
    raise KeyError((scope_name, form_name, category))


def gaps() -> list[LedgerRow]:
    return [row for row in build_rows() if row.outcome == "gap"]


if __name__ == "__main__":
    validate()
    print(f"{len(build_rows())} route rows")
    for row in gaps():
        print(f"gap {row.gap}: {row.scope}/{row.form}/{row.category}")
