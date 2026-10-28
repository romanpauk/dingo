#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

"""The declared exceptions to the model's derived coverage.

A gap is a storage form or shape the library publishes but a family cannot yet execute.
Every entry carries a reason, and the families that consume a table still check
that a declared gap is required. Keeping the tables together makes the full
exception surface visible in one place.
"""

from __future__ import annotations

from dataclasses import dataclass

from spec.grammar.catalog import CONSUMING_SCOPES


@dataclass(frozen=True)
class Gap:
    owner: str
    key: str | tuple[str, ...]
    reason: str


GAPS = (
    # Behavior forms that the route ledger publishes but that have no
    # resolvable behavior case yet.
    Gap(
        "behavior",
        ("shared", "unique_handle"),
        "the published unique-handle value route is not convertible",
    ),
    Gap(
        "behavior",
        ("external", "unique_handle"),
        "external storage publishes no resolvable unique-handle route",
    ),
    Gap(
        "behavior",
        ("external", "unique_array_handle"),
        "external storage publishes no resolvable array-handle route",
    ),
    Gap(
        "behavior",
        ("shared_cyclical", "alternative"),
        "the cyclical alternative form needs the graph scenario family",
    ),
    Gap(
        "behavior",
        ("shared_cyclical", "pointer"),
        "the cyclical pointer form needs the graph scenario family",
    ),
    # Forms with no interface behavior scenario.
    Gap("interface", "alternative", "variant storage does not publish an interface route"),
    Gap("interface", "pointer", "raw pointer storage does not publish an interface route"),
    Gap(
        "interface",
        "bounded_array",
        "bounded-array storage does not publish an interface route",
    ),
    Gap(
        "interface",
        "unique_array_handle",
        "array-handle storage does not publish an interface route",
    ),
    Gap(
        "interface",
        "shared_array_handle",
        "array-handle storage does not publish an interface route",
    ),
    Gap("interface", "optional", "optional storage does not publish an interface route"),
    # Forms with no annotation scenario.
    Gap("annotation", "alternative", "variant storage has no annotation scenario"),
    Gap("annotation", "bounded_array", "array storage has no annotation scenario"),
    Gap("annotation", "unique_handle", "unique handle storage has no annotation scenario"),
    Gap(
        "annotation",
        "unique_array_handle",
        "array handle storage has no annotation scenario",
    ),
    Gap(
        "annotation",
        "shared_array_handle",
        "array handle storage has no annotation scenario",
    ),
    # Storage forms that publish no route and whose storage type cannot be named by an
    # absence probe.
    Gap("absence", ("shared", "array"), "raw dynamic array storage cannot be instantiated"),
    Gap("absence", ("unique", "array"), "raw dynamic array storage cannot be instantiated"),
    Gap("absence", ("external", "array"), "raw dynamic array storage cannot be instantiated"),
    Gap(
        "absence",
        ("shared_cyclical", "array"),
        "raw dynamic array storage cannot be instantiated",
    ),
    Gap(
        "absence",
        ("shared_cyclical", "unique_handle"),
        "shared_cyclical cannot instantiate the form's storage type",
    ),
    Gap(
        "absence",
        ("shared_cyclical", "unique_array_handle"),
        "shared_cyclical cannot instantiate the form's storage type",
    ),
    Gap(
        "absence",
        ("shared_cyclical", "shared_array_handle"),
        "shared_cyclical cannot instantiate the form's storage type",
    ),
    Gap(
        "absence",
        ("shared_cyclical", "optional"),
        "shared_cyclical cannot instantiate the form's storage type",
    ),
)


def form_gaps(owner: str) -> dict[tuple[str, str], str]:
    return {
        gap.key: gap.reason
        for gap in GAPS
        if gap.owner == owner and isinstance(gap.key, tuple)
    }


def name_gaps(owner: str) -> dict[str, str]:
    return {
        gap.key: gap.reason
        for gap in GAPS
        if gap.owner == owner and isinstance(gap.key, str)
    }


def validate() -> None:
    keys = [(gap.owner, gap.key) for gap in GAPS]
    if len(keys) != len(set(keys)):
        raise ValueError("gap registry contains duplicate keys")
    for gap in GAPS:
        if not gap.reason:
            raise ValueError(f"gap {gap.owner}/{gap.key} has no reason")


def declared_runtime_gap(
    binding: str, scope: str, form_kind: str, access: str
) -> str | None:
    """A supported path the runtime families deliberately do not execute.

    The route is published, but a top-level runtime probe would either be
    covered by a representative value route, require the graph scenario family,
    or outlive the resolution transaction. The report exposes these separately
    from missing tests.
    """
    if binding == "interface" and scope in CONSUMING_SCOPES:
        return (
            "consuming-scope interface registration materializes an abstract "
            "interface value"
        )
    if binding != "concrete":
        return None
    if form_kind in {"handle", "array_handle"} and access in {
        "lvalue_reference",
        "pointer",
    }:
        return "top-level handle access is covered by the handle value route"
    if (
        scope == "shared_cyclical"
        and form_kind == "pointer"
        and access in {"lvalue_reference", "pointer"}
    ):
        return "cyclical pointer access is covered by the graph scenario family"
    if (
        scope in CONSUMING_SCOPES
        and form_kind == "value"
        and access == "lvalue_reference"
    ):
        return "top-level lvalue borrow from a consuming scope outlives the resolution"
    return None
