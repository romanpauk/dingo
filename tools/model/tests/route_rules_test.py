#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import pytest

from tools.model.semantics.routes.rules import derive_route, reason_text
from tools.model.grammar.shapes import SHAPES


SHAPE_BY_NAME = {shape.name: shape for shape in SHAPES}

RULES = {
    "access",
    "borrow",
    "move",
    "copy_out",
    "take",
    "wrap",
    "share",
    "adopt",
    "decay",
    "select_alternative",
    "nested_access",
    "const_copy",
    "target",
}


def reason(scope: str, form: str, shape: str) -> str:
    return reason_text(scope, form, SHAPE_BY_NAME[shape])


def test_access_rules_are_named() -> None:
    assert reason("shared", "plain", "rvalue_reference_value").startswith("move:")
    assert reason("unique", "plain", "pointer_value").startswith("borrow:")
    assert reason("unique", "alternative", "lvalue_reference_value").startswith(
        "borrow:"
    )
    assert reason(
        "shared", "plain", "array_pointer_value"
    ).startswith("decay:")
    assert reason(
        "unique", "bounded_array", "array_lvalue_reference_value"
    ).startswith(("borrow:", "decay:"))


def test_wrapper_rules_are_named() -> None:
    assert reason("shared", "plain", "shared_ptr_value").startswith("take:")
    assert reason("shared", "plain", "unique_ptr_value").startswith("take:")

    assert reason(
        "shared", "plain", "pointer_pointer_value"
    ).startswith("nested_access:")
    assert reason(
        "shared", "plain", "optional_pointer_value"
    ).startswith("nested_access:")
    assert reason(
        "shared", "plain", "optional_optional_value"
    ).startswith("wrap:")
    assert reason(
        "shared", "plain", "optional_const_value"
    ).startswith("const_copy:")
    assert reason(
        "shared", "plain", "variant_value"
    ).startswith("select_alternative:")


def test_rules_accept_what_the_cell_stores() -> None:
    assert derive_route(
        "shared", "plain", SHAPE_BY_NAME["value"]
    ) is None
    assert derive_route(
        "shared", "optional", SHAPE_BY_NAME["optional_value"]
    ) is None
    assert derive_route(
        "shared", "shared_handle", SHAPE_BY_NAME["shared_ptr_value"]
    ) is None
    assert derive_route(
        "unique", "unique_handle", SHAPE_BY_NAME["unique_ptr_value"]
    ) is None
    assert derive_route(
        "shared", "alternative", SHAPE_BY_NAME["variant_value"]
    ) is None
    assert derive_route(
        "shared", "bounded_array", SHAPE_BY_NAME["array_pointer_value"]
    ) is None
    assert derive_route(
        "shared_cyclical", "bounded_array", SHAPE_BY_NAME["pointer_value"]
    ) is None


def test_every_reason_uses_a_known_rule() -> None:
    for scope in ("shared", "unique", "external", "shared_cyclical"):
        for form in (
            "plain",
            "alternative",
            "pointer",
            "bounded_array",
            "unique_handle",
            "shared_handle",
            "optional",
        ):
            for shape in SHAPES:
                text = reason_text(scope, form, shape)
                assert text
                assert text.split(":", 1)[0] in RULES


if __name__ == "__main__":
    raise SystemExit(pytest.main([__file__]))
