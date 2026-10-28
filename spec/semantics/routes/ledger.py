#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The explicit route ledger for the built-in storage forms.

Every scope, storage form, and route category has exactly one outcome:

- supported: the storage form publishes the category;
- unavailable: the scope/form combination intentionally cannot publish it;
- gap: the route shape is not modeled yet, with a tracked gap id.

No row may be absent, and no gap is silent. The ledger is a cached view over
the catalog and the route rules; it does not keep a second copy of either.
"""

from __future__ import annotations

from dataclasses import dataclass
from functools import lru_cache

from spec.grammar.catalog import FORM_BY_NAME, FORMS, SCOPE_BY_NAME, SCOPES
from spec.semantics.routes.derivation import RouteCandidate, route_candidates
from spec.semantics.forms import LVALUE, POINTER, RVALUE, VALUE, exposure


CATEGORIES = ("value", "object", "rvalue")

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


@dataclass(frozen=True)
class LedgerRow:
    scope: str
    form: str
    category: str
    outcome: str
    reason: str = ""
    gap: str = ""
    access: str = ""
    candidates: tuple[RouteCandidate, ...] = ()

    @property
    def kinds(self) -> tuple[str, ...]:
        return tuple(candidate.kind for candidate in self.candidates)

    @property
    def retention(self) -> tuple[int, ...]:
        return tuple(int(candidate.retention) for candidate in self.candidates)

    @property
    def steps(self) -> tuple[tuple[str, ...], ...]:
        return tuple(candidate.steps for candidate in self.candidates)

    @property
    def targets(self) -> tuple[str, ...]:
        return tuple(candidate.target for candidate in self.candidates)


def _access(scope_name: str, category: str) -> str:
    if category == "rvalue":
        return "consume"
    if category == "value" and SCOPE_BY_NAME[scope_name].consumes:
        return "consume"
    return "borrow"


def _category_outcome(
    scope_name: str, form_name: str, bits: int, category: str
) -> LedgerRow:
    if form_name == "array":
        return LedgerRow(
            scope=scope_name,
            form=form_name,
            category=category,
            outcome="unavailable",
            reason=RAW_ARRAY_UNAVAILABLE,
        )
    if bits == 0:
        return LedgerRow(
            scope=scope_name,
            form=form_name,
            category=category,
            outcome="unavailable",
            reason="scope does not publish the form",
        )
    unavailable = ARRAY_UNAVAILABLE.get((scope_name, form_name))
    if unavailable is not None:
        return LedgerRow(
            scope=scope_name,
            form=form_name,
            category=category,
            outcome="unavailable",
            reason=unavailable,
        )
    form = FORM_BY_NAME[form_name]
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
        scope=scope_name,
        form=form_name,
        category=category,
        outcome="supported" if supported else "unavailable",
        reason="" if supported else reason,
        access=_access(scope_name, category) if supported else "",
        candidates=(
            route_candidates(scope_name, form_name, category)
            if supported
            else ()
        ),
    )


@lru_cache(maxsize=1)
def build_rows() -> tuple[LedgerRow, ...]:
    rows = []
    for scope in SCOPES:
        for form in FORMS:
            bits = exposure(scope, form)
            for category in CATEGORIES:
                rows.append(
                    _category_outcome(scope.name, form.name, bits, category)
                )
    return tuple(rows)


@lru_cache(maxsize=1)
def _rows_by_key() -> dict[tuple[str, str, str], LedgerRow]:
    return {(row.scope, row.form, row.category): row for row in build_rows()}


@lru_cache(maxsize=1)
def _supported() -> dict[tuple[str, str], frozenset[str]]:
    supported: dict[tuple[str, str], set[str]] = {}
    for row in build_rows():
        if row.outcome == "supported":
            supported.setdefault((row.scope, row.form), set()).add(row.category)
    return {key: frozenset(value) for key, value in supported.items()}


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
        if row.outcome == "supported" and not row.candidates:
            raise ValueError(f"supported row has no candidates: {row}")
        if row.outcome != "supported" and row.candidates:
            raise ValueError(f"non-supported row has candidates: {row}")
        for candidate in row.candidates:
            if not candidate.kind:
                raise ValueError(f"candidate has no operation kind: {row}")
            if candidate.retention not in (0, 1):
                raise ValueError(f"candidate retention is not boolean: {row}")
            if not candidate.steps:
                raise ValueError(f"candidate has no conversion steps: {row}")
            if not candidate.target:
                raise ValueError(f"candidate has no target shape: {row}")


def supported_categories(scope_name: str, form_name: str) -> set[str]:
    return set(_supported().get((scope_name, form_name), frozenset()))


def _supported_row(
    scope_name: str, form_name: str, category: str
) -> LedgerRow:
    row = _rows_by_key().get((scope_name, form_name, category))
    if row is None or row.outcome != "supported":
        raise KeyError((scope_name, form_name, category))
    return row


def category_access(scope_name: str, form_name: str, category: str) -> str:
    return _supported_row(scope_name, form_name, category).access


def category_candidates(
    scope_name: str, form_name: str, category: str
) -> tuple[RouteCandidate, ...]:
    return _supported_row(scope_name, form_name, category).candidates


def category_kinds(
    scope_name: str, form_name: str, category: str
) -> tuple[str, ...]:
    return _supported_row(scope_name, form_name, category).kinds


def category_retention(
    scope_name: str, form_name: str, category: str
) -> tuple[int, ...]:
    return _supported_row(scope_name, form_name, category).retention


def category_steps(
    scope_name: str, form_name: str, category: str
) -> tuple[tuple[str, ...], ...]:
    return _supported_row(scope_name, form_name, category).steps


def category_targets(
    scope_name: str, form_name: str, category: str
) -> tuple[str, ...]:
    return _supported_row(scope_name, form_name, category).targets


def gaps() -> list[LedgerRow]:
    return [row for row in build_rows() if row.outcome == "gap"]


if __name__ == "__main__":
    validate()
    print(f"{len(build_rows())} route rows")
    for row in gaps():
        print(f"gap {row.gap}: {row.scope}/{row.form}/{row.category}")
