"""Tests of the C++ spelling parser of test/routes/conform.py."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

import conform  # noqa: E402
from spec.reference.routes import RequestForm, TypeTree, wrapper, leaf  # noqa: E402

LEAF = leaf(conform.LEAVES["leaf"])


@pytest.mark.parametrize(
    "spelling, kind, type_tree",
    [
        ("leaf", RequestForm.VALUE, LEAF),
        ("const leaf&", RequestForm.LVALUE_REFERENCE, LEAF),
        ("leaf&&", RequestForm.RVALUE_REFERENCE, LEAF),
        ("const leaf*", RequestForm.VALUE, wrapper("ptr", LEAF)),
        ("leaf* const&", RequestForm.LVALUE_REFERENCE, wrapper("ptr", LEAF)),
        ("leaf**", RequestForm.VALUE, wrapper("ptr", wrapper("ptr", LEAF))),
        ("leaf(&)[2]", RequestForm.LVALUE_REFERENCE, wrapper("array", LEAF, extent=2)),
        ("const leaf(*)[2]", RequestForm.VALUE, wrapper("ptr", wrapper("array", LEAF, extent=2))),
        ("std::shared_ptr<leaf[]>&", RequestForm.LVALUE_REFERENCE,
         wrapper("share", wrapper("array_unbounded", LEAF))),
        ("std::variant<leaf, other_leaf>", RequestForm.VALUE,
         wrapper("alt", LEAF, leaf(conform.LEAVES["other_leaf"]))),
        ("std::unique_ptr<std::optional<leaf>>*", RequestForm.VALUE,
         wrapper("ptr", wrapper("own", wrapper("opt", LEAF)))),
        ("user_shared<leaf>&", RequestForm.LVALUE_REFERENCE,
         wrapper("user_share", LEAF, family="user")),
        ("std::shared_ptr<user_optional<leaf>>", RequestForm.VALUE,
         wrapper("share", wrapper("user_opt", LEAF, family="user"))),
        ("const iface*", RequestForm.VALUE, wrapper("ptr", leaf(conform.LEAVES["iface"]))),
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
