"""Cases of the reference graph rules."""

import pytest

import spec
from spec.reference.graph import Node, illegal_cycle


@pytest.fixture(scope="module")
def loaded():
    return spec.load_spec()


def test_a_cycle_of_shared_bindings_is_illegal(loaded):
    nodes = [
        Node("left", "shared", ("right",)),
        Node("right", "shared", ("left",)),
    ]
    assert illegal_cycle(loaded, nodes) == ["left", "right", "left"]


def test_a_cycle_of_shared_cyclical_bindings_is_legal(loaded):
    nodes = [
        Node("left", "shared_cyclical", ("right",)),
        Node("right", "shared_cyclical", ("left",)),
    ]
    assert illegal_cycle(loaded, nodes) is None


def test_one_binding_that_is_not_cyclical_makes_a_cycle_illegal(loaded):
    nodes = [
        Node("left", "shared_cyclical", ("right",)),
        Node("right", "shared", ("left",)),
    ]
    assert illegal_cycle(loaded, nodes) == ["right", "left", "right"]


def test_a_binding_that_depends_on_itself(loaded):
    assert illegal_cycle(loaded, [Node("self", "unique", ("self",))]) == [
        "self",
        "self",
    ]


def test_the_shortest_cycle_is_reported(loaded):
    nodes = [
        Node("a", "shared", ("b", "c")),
        Node("b", "shared", ("c",)),
        Node("c", "shared", ("a",)),
    ]
    assert illegal_cycle(loaded, nodes) == ["a", "c", "a"]


def test_a_chain_without_a_cycle(loaded):
    nodes = [Node("a", "shared", ("b",)), Node("b", "shared", ())]
    assert illegal_cycle(loaded, nodes) is None
