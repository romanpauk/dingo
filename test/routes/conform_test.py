"""Tests of the C++ spelling parser of test/routes/conform.py."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

import conform  # noqa: E402
from spec.reference.routes import RequestKind, TypeTree, wrapper, leaf  # noqa: E402

LEAF = leaf(conform.LEAVES["leaf"])


@pytest.mark.parametrize(
    "spelling, kind, type_tree",
    [
        ("leaf", RequestKind.VALUE, LEAF),
        ("const leaf&", RequestKind.LVALUE_REFERENCE, LEAF),
        ("leaf&&", RequestKind.RVALUE_REFERENCE, LEAF),
        ("const leaf*", RequestKind.VALUE, wrapper("ptr", LEAF)),
        ("leaf* const&", RequestKind.LVALUE_REFERENCE, wrapper("ptr", LEAF)),
        ("leaf**", RequestKind.VALUE, wrapper("ptr", wrapper("ptr", LEAF))),
        ("leaf(&)[2]", RequestKind.LVALUE_REFERENCE, wrapper("array", LEAF, extent=2)),
        ("const leaf(*)[2]", RequestKind.VALUE, wrapper("ptr", wrapper("array", LEAF, extent=2))),
        ("std::shared_ptr<leaf[]>&", RequestKind.LVALUE_REFERENCE,
         wrapper("share", wrapper("array_unbounded", LEAF))),
        ("std::variant<leaf, other_leaf>", RequestKind.VALUE,
         wrapper("alt", LEAF, leaf(conform.LEAVES["other_leaf"]))),
        ("std::unique_ptr<std::optional<leaf>>*", RequestKind.VALUE,
         wrapper("ptr", wrapper("own", wrapper("opt", LEAF)))),
        ("user_shared<leaf>&", RequestKind.LVALUE_REFERENCE,
         wrapper("user_share", LEAF, family="user")),
        ("std::shared_ptr<user_optional<leaf>>", RequestKind.VALUE,
         wrapper("share", wrapper("user_opt", LEAF, family="user"))),
        ("const iface*", RequestKind.VALUE, wrapper("ptr", leaf(conform.LEAVES["iface"]))),
    ],
)
def test_parse_request(spelling, kind, type_tree):
    request = conform.parse_request(spelling)
    assert request.kind == kind
    assert request.type_tree == type_tree


def test_parse_stored_reference():
    assert conform.parse_type_tree("leaf&") == wrapper("ref", LEAF)
    assert conform.parse_type_tree("leaf[2]") == wrapper("array", LEAF, extent=2)


@pytest.mark.skipif(
    conform.default_observed() is None,
    reason="no route table extracted under build/routes/observed",
)
def test_the_observed_table_conforms():
    assert conform.main_with([]) == 0
