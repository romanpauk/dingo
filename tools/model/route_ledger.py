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
    "bounded_array",
    "unique_handle",
    "unique_array_handle",
    "shared_handle",
    "shared_array_handle",
    "optional",
)
GAP_ARRAY_ROUTES = "array-routes"

# Constraints that are not gaps: the shape is declared, but the combination is
# rejected for a concrete reason.
ARRAY_UNAVAILABLE = {
    (
        "shared_cyclical",
        "shared_array_handle",
    ): "shared_cyclical cannot allocate an unsized array handle",
}

# The raw dynamic array form has no element count, so it can never be sized,
# constructed, or destroyed by a scope. It is declared unavailable rather than
# a tracked gap.
RAW_ARRAY_UNAVAILABLE = (
    "raw dynamic array storage is not supported: the element count is not part "
    "of the type"
)

OPERATION_KINDS = {
    ("shared", "plain", "value"): ("dereference",),
    ("shared", "plain", "object"): ("traits",),
    ("shared", "alternative", "value"): ("dereference",),
    ("shared", "alternative", "object"): ("dereference", "borrowed"),
    ("shared", "pointer", "value"): ("dereference",),
    ("shared", "pointer", "object"): ("traits",),
    ("shared", "unique_handle", "value"): ("borrowed",),
    ("shared", "unique_handle", "object"): (
        "address",
        "borrowed",
        "pointer_access",
    ),
    ("shared", "shared_handle", "value"): ("borrowed", "identity"),
    ("shared", "shared_handle", "object"): (
        "address",
        "borrowed",
        "pointer_access",
    ),
    ("shared", "optional", "value"): ("borrowed",),
    ("shared", "optional", "object"): (
        "borrowed",
        "traits",
        "pointer_access",
        "address",
    ),
    ("unique", "plain", "value"): ("identity", "wrapper"),
    ("unique", "plain", "object"): ("traits",),
    ("unique", "plain", "rvalue"): ("identity", "wrapper"),
    ("unique", "alternative", "value"): ("alternative",),
    ("unique", "alternative", "rvalue"): ("alternative",),
    ("unique", "pointer", "value"): ("traits", "traits"),
    ("unique", "pointer", "object"): ("traits",),
    ("unique", "pointer", "rvalue"): ("traits", "traits"),
    ("unique", "unique_handle", "value"): ("identity", "traits"),
    ("unique", "unique_handle", "rvalue"): ("identity", "traits"),
    ("unique", "shared_handle", "value"): ("identity",),
    ("unique", "shared_handle", "rvalue"): ("identity",),
    ("unique", "optional", "value"): ("identity",),
    ("unique", "optional", "rvalue"): ("identity",),
    ("external", "plain", "value"): ("identity",),
    ("external", "plain", "object"): ("address",),
    ("external", "alternative", "value"): ("alternative",),
    ("external", "alternative", "object"): ("alternative", "alternative"),
    ("external", "pointer", "value"): ("dereference",),
    ("external", "pointer", "object"): ("traits",),
    ("external", "unique_handle", "value"): ("borrowed",),
    ("external", "unique_handle", "object"): (
        "address",
        "borrowed",
        "pointer_access",
    ),
    ("external", "shared_handle", "value"): ("borrowed", "identity"),
    ("external", "shared_handle", "object"): (
        "address",
        "borrowed",
        "pointer_access",
    ),
    ("external", "optional", "value"): ("borrowed",),
    ("external", "optional", "object"): (
        "borrowed",
        "traits",
        "pointer_access",
        "address",
    ),
    ("shared_cyclical", "plain", "object"): ("traits",),
    ("shared_cyclical", "alternative", "object"): (
        "dereference",
        "borrowed",
    ),
    ("shared_cyclical", "pointer", "object"): ("dereference", "borrowed"),
    ("shared_cyclical", "shared_handle", "value"): ("identity",),
    ("shared_cyclical", "shared_handle", "object"): (
        "address",
        "borrowed",
        "pointer_access",
    ),
    ("shared_cyclical", "bounded_array", "object"): (
        "dereference",
        "traits",
    ),
    ("shared", "bounded_array", "object"): (
        "array_reference",
        "traits",
        "array_pointer",
    ),
    ("unique", "bounded_array", "value"): ("traits", "traits"),
    ("unique", "bounded_array", "object"): ("traits", "array_pointer"),
    ("unique", "bounded_array", "rvalue"): ("traits", "traits"),
    ("external", "bounded_array", "object"): (
        "array_reference",
        "traits",
        "array_pointer",
    ),
    ("shared", "unique_array_handle", "object"): (
        "address",
        "pointer_access",
    ),
    ("unique", "unique_array_handle", "value"): ("identity", "traits"),
    ("unique", "unique_array_handle", "rvalue"): ("identity", "traits"),
    ("shared", "shared_array_handle", "value"): ("identity",),
    ("shared", "shared_array_handle", "object"): (
        "address",
        "pointer_access",
    ),
    ("unique", "shared_array_handle", "value"): ("identity",),
    ("unique", "shared_array_handle", "rvalue"): ("identity",),
    ("external", "unique_array_handle", "object"): (
        "address",
        "pointer_access",
    ),
    ("external", "shared_array_handle", "value"): ("identity",),
    ("external", "shared_array_handle", "object"): (
        "address",
        "pointer_access",
    ),
}

RETENTION = {
    ("shared", "plain", "value"): (0,),
    ("shared", "plain", "object"): (0,),
    ("shared", "alternative", "value"): (1,),
    ("shared", "alternative", "object"): (1, 1),
    ("shared", "pointer", "value"): (0,),
    ("shared", "pointer", "object"): (0,),
    ("shared", "unique_handle", "value"): (1,),
    ("shared", "unique_handle", "object"): (0, 1, 1),
    ("shared", "shared_handle", "value"): (1, 0),
    ("shared", "shared_handle", "object"): (0, 1, 1),
    ("shared", "optional", "value"): (1,),
    ("shared", "optional", "object"): (1, 0, 1, 0),
    ("unique", "plain", "value"): (0, 1),
    ("unique", "plain", "object"): (0,),
    ("unique", "plain", "rvalue"): (0, 1),
    ("unique", "alternative", "value"): (1,),
    ("unique", "alternative", "rvalue"): (1,),
    ("unique", "pointer", "value"): (1, 1),
    ("unique", "pointer", "object"): (0,),
    ("unique", "pointer", "rvalue"): (1, 1),
    ("unique", "unique_handle", "value"): (0, 1),
    ("unique", "unique_handle", "rvalue"): (0, 1),
    ("unique", "shared_handle", "value"): (0,),
    ("unique", "shared_handle", "rvalue"): (0,),
    ("unique", "optional", "value"): (0,),
    ("unique", "optional", "rvalue"): (0,),
    ("external", "plain", "value"): (0,),
    ("external", "plain", "object"): (0,),
    ("external", "alternative", "value"): (1,),
    ("external", "alternative", "object"): (1, 1),
    ("external", "pointer", "value"): (0,),
    ("external", "pointer", "object"): (0,),
    ("external", "unique_handle", "value"): (1,),
    ("external", "unique_handle", "object"): (0, 1, 1),
    ("external", "shared_handle", "value"): (1, 0),
    ("external", "shared_handle", "object"): (0, 1, 1),
    ("external", "optional", "value"): (1,),
    ("external", "optional", "object"): (1, 0, 1, 0),
    ("shared_cyclical", "plain", "object"): (0,),
    ("shared_cyclical", "alternative", "object"): (1, 1),
    ("shared_cyclical", "pointer", "object"): (1, 1),
    ("shared_cyclical", "shared_handle", "value"): (0,),
    ("shared_cyclical", "shared_handle", "object"): (0, 1, 1),
    ("shared_cyclical", "bounded_array", "object"): (0, 0),
    ("shared", "bounded_array", "object"): (1, 0, 1),
    ("unique", "bounded_array", "value"): (1, 1),
    ("unique", "bounded_array", "object"): (0, 1),
    ("unique", "bounded_array", "rvalue"): (1, 1),
    ("external", "bounded_array", "object"): (1, 0, 1),
    ("shared", "unique_array_handle", "object"): (0, 1),
    ("unique", "unique_array_handle", "value"): (0, 1),
    ("unique", "unique_array_handle", "rvalue"): (0, 1),
    ("shared", "shared_array_handle", "value"): (0,),
    ("shared", "shared_array_handle", "object"): (0, 1),
    ("unique", "shared_array_handle", "value"): (0,),
    ("unique", "shared_array_handle", "rvalue"): (0,),
    ("external", "unique_array_handle", "object"): (0, 1),
    ("external", "shared_array_handle", "value"): (0,),
    ("external", "shared_array_handle", "object"): (0, 1),
}


@dataclass(frozen=True)
class LedgerRow:
    scope: str
    form: str
    category: str
    outcome: str
    reason: str = ""
    gap: str = ""
    access: str = ""
    kinds: tuple[str, ...] = ()
    kind_gap: str = ""
    retention: tuple[int, ...] = ()


def _category_outcome(
    scope_name: str, form_name: str, bits: int, category: str
) -> LedgerRow:
    scope = next(scope for scope in SCOPES if scope.name == scope_name)
    form = next(form for form in FORMS if form.name == form_name)
    if form_name == "array":
        return LedgerRow(
            scope_name,
            form_name,
            category,
            "unavailable",
            RAW_ARRAY_UNAVAILABLE,
        )
    if bits == 0:
        return LedgerRow(
            scope_name,
            form_name,
            category,
            "unavailable",
            "scope does not publish the form",
        )
    unavailable = ARRAY_UNAVAILABLE.get((scope_name, form_name))
    if unavailable is not None:
        return LedgerRow(
            scope_name,
            form_name,
            category,
            "unavailable",
            unavailable,
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
        OPERATION_KINDS.get((scope_name, form_name, category), ())
        if supported
        else (),
        "",
        RETENTION.get((scope_name, form_name, category), ()) if supported else (),
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
        if row.outcome == "supported" and not row.kinds:
            raise ValueError(f"supported row has no operation kinds: {row}")
        if row.outcome != "supported" and row.kinds:
            raise ValueError(f"non-supported row has operation kinds: {row}")
        if len(row.retention) != len(row.kinds):
            raise ValueError(f"retention sequence length mismatch: {row}")
        if any(value not in (0, 1) for value in row.retention):
            raise ValueError(f"retention value is not boolean: {row}")
        if row.kind_gap:
            raise ValueError(f"operation-kind gap remains: {row}")


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


def category_kinds(
    scope_name: str, form_name: str, category: str
) -> tuple[str, ...]:
    for row in build_rows():
        if (
            row.scope == scope_name
            and row.form == form_name
            and row.category == category
            and row.outcome == "supported"
        ):
            return row.kinds
    raise KeyError((scope_name, form_name, category))


def category_retention(
    scope_name: str, form_name: str, category: str
) -> tuple[int, ...]:
    for row in build_rows():
        if (
            row.scope == scope_name
            and row.form == form_name
            and row.category == category
            and row.outcome == "supported"
        ):
            return row.retention
    raise KeyError((scope_name, form_name, category))


def gaps() -> list[LedgerRow]:
    return [row for row in build_rows() if row.outcome == "gap"]


if __name__ == "__main__":
    validate()
    print(f"{len(build_rows())} route rows")
    for row in gaps():
        print(f"gap {row.gap}: {row.scope}/{row.form}/{row.category}")
