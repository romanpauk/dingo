"""Tests of the C++ spelling parser of test/routes/conform.py."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

import conform  # noqa: E402
from spec.reference.routes import RequestKind, Shape, layer, leaf  # noqa: E402

LEAF = leaf(conform.LEAVES["leaf"])


@pytest.mark.parametrize(
    "spelling, kind, shape",
    [
        ("leaf", RequestKind.VALUE, LEAF),
        ("const leaf&", RequestKind.LVALUE_REFERENCE, LEAF),
        ("leaf&&", RequestKind.RVALUE_REFERENCE, LEAF),
        ("const leaf*", RequestKind.VALUE, layer("ptr", LEAF)),
        ("leaf* const&", RequestKind.LVALUE_REFERENCE, layer("ptr", LEAF)),
        ("leaf**", RequestKind.VALUE, layer("ptr", layer("ptr", LEAF))),
        ("leaf(&)[2]", RequestKind.LVALUE_REFERENCE, layer("array", LEAF, extent=2)),
        ("const leaf(*)[2]", RequestKind.VALUE, layer("ptr", layer("array", LEAF, extent=2))),
        ("std::shared_ptr<leaf[]>&", RequestKind.LVALUE_REFERENCE,
         layer("share", layer("array_unbounded", LEAF))),
        ("std::variant<leaf, other_leaf>", RequestKind.VALUE,
         layer("alt", LEAF, leaf(conform.LEAVES["other_leaf"]))),
        ("std::unique_ptr<std::optional<leaf>>*", RequestKind.VALUE,
         layer("ptr", layer("own", layer("opt", LEAF)))),
        ("user_shared<leaf>&", RequestKind.LVALUE_REFERENCE,
         layer("share", LEAF, family="user")),
        ("std::shared_ptr<user_optional<leaf>>", RequestKind.VALUE,
         layer("share", layer("opt", LEAF, family="user"))),
        ("const iface*", RequestKind.VALUE, layer("ptr", leaf(conform.LEAVES["iface"]))),
    ],
)
def test_parse_request(spelling, kind, shape):
    request = conform.parse_request(spelling)
    assert request.kind == kind
    assert request.shape == shape


def test_parse_stored_reference():
    assert conform.parse_shape("leaf&") == layer("ref", LEAF)
    assert conform.parse_shape("leaf[2]") == layer("array", LEAF, extent=2)


@pytest.mark.skipif(
    conform.default_observed() is None,
    reason="no route table extracted under build/routes/observed",
)
def test_the_observed_table_conforms():
    assert conform.main_with([]) == 0
