#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import pytest

from route_ledger import build_rows, supported_categories
from route_recipes import (
    interface_route_counts,
    route_kinds,
    route_retention,
    route_steps,
    source_shape,
)
from storage_cells import FORMS, SCOPES


def test_every_supported_cell_has_a_route_script() -> None:
    for row in build_rows():
        if row.outcome != "supported":
            continue
        steps = route_steps(row.scope, row.form, row.category)
        assert steps
        assert tuple(step.kind for step in steps) == row.kinds
        assert tuple(
            int(step.retention) for step in steps
        ) == row.retention


def test_source_shape_describes_how_the_scope_hands_out_the_value() -> None:
    assert source_shape("shared", "plain") == "pointer"
    assert source_shape("shared", "optional") == "lvalue"
    assert source_shape("external", "plain") == "lvalue"
    assert source_shape("unique", "plain") == "rvalue"
    assert source_shape("unique", "pointer") == "rvalue"
    assert source_shape("shared_cyclical", "plain") == "pointer"
    assert source_shape("shared_cyclical", "pointer") == "cyclical_double"
    assert source_shape("shared_cyclical", "shared_handle") == "lvalue_cyclical"


def test_value_routes_follow_the_source_shape() -> None:
    assert route_kinds("shared", "plain", "value") == ("dereference",)
    assert route_kinds("external", "plain", "value") == ("identity",)
    assert route_kinds("unique", "plain", "value") == ("identity", "wrapper")
    assert route_kinds("shared", "alternative", "value") == ("dereference",)
    assert route_kinds("external", "alternative", "value") == (
        "alternative",
    )
    assert route_kinds("unique", "alternative", "value") == (
        "alternative",
    )


def test_object_routes_follow_the_stored_wrapper() -> None:
    assert route_kinds("shared", "plain", "object") == ("traits",)
    assert route_kinds("external", "plain", "object") == ("address",)
    assert route_kinds("shared", "shared_handle", "object") == (
        "address",
        "borrowed",
        "pointer_access",
    )
    assert route_kinds("shared", "optional", "object") == (
        "borrowed",
        "traits",
        "pointer_access",
        "address",
    )
    assert route_kinds("unique", "alternative", "rvalue") == (
        "alternative",
    )


def test_retention_is_part_of_the_route_script() -> None:
    assert route_retention("shared", "plain", "value") == (0,)
    assert route_retention("shared", "alternative", "value") == (1,)
    assert route_retention("unique", "plain", "value") == (0, 1)
    assert route_retention("shared", "optional", "object") == (1, 0, 1, 0)
    assert route_retention("shared", "shared_handle", "value") == (1, 1, 0)


def test_every_supported_route_has_an_access() -> None:
    for row in build_rows():
        if row.outcome == "supported":
            assert row.access in {"borrow", "consume"}
            assert len(row.retention) == len(row.kinds)


def test_unmodeled_cell_forms_fail_loudly() -> None:
    with pytest.raises(KeyError):
        route_steps("shared", "unknown_form", "value")
    with pytest.raises(KeyError):
        route_steps("shared", "plain", "unknown_category")


def test_route_scripts_cover_every_supported_cell_category() -> None:
    for scope in SCOPES:
        for form in FORMS:
            for category in supported_categories(scope.name, form.name):
                assert route_steps(scope.name, form.name, category)


def test_interface_route_counts_follow_the_form() -> None:
    assert interface_route_counts("shared", "shared_handle") == {
        "value": 2,
        "object": 4,
        "rvalue": 0,
    }
    assert interface_route_counts(
        "shared", "shared_handle", copyable_value=True
    ) == {"value": 3, "object": 4, "rvalue": 0}
    assert interface_route_counts("unique", "unique_handle") == {
        "value": 4,
        "object": 0,
        "rvalue": 4,
    }
    assert interface_route_counts("shared", "plain") == {
        "value": 0,
        "object": 1,
        "rvalue": 0,
    }
    with pytest.raises(KeyError):
        interface_route_counts("shared", "optional")


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
