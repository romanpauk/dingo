"""Cases and properties of the reference selection implementation."""

import itertools

import pytest

import spec
from spec.reference.selection import NO_KEY, Binding, Key, Level, Model, Request
from spec.schema import Cardinality, Status

TAG = Key("tag", "tag")
OTHER_TAG = Key("tag", "other_tag")
SEVEN = Key("runtime", "int", 7)
FIXED_SEVEN = Key("fixed", "int", 7)
EIGHT = Key("runtime", "int", 8)
KEYS = (NO_KEY, TAG, OTHER_TAG, SEVEN, FIXED_SEVEN, EIGHT)


@pytest.fixture(scope="module")
def model():
    return Model(spec.load_spec())


def level(model, *bindings, parent=None, many=()):
    result = Level(parent=parent)
    for interface, key in many:
        result.declarations[(interface, key.domain())] = Cardinality.MANY
    for binding in bindings:
        assert model.register(result, binding) == (True, "")
    return result


def names(selection):
    return [binding.name for binding in selection.bindings]


def test_a_request_names_the_bindings_of_its_interface_and_key(model):
    container = level(
        model,
        Binding("plain", ("iface",)),
        Binding("tagged", ("iface",), TAG),
        Binding("seven", ("iface",), SEVEN),
    )
    assert names(model.select(container, Request("iface"))) == ["plain"]
    assert names(model.select(container, Request("iface", TAG))) == ["tagged"]
    assert names(model.select(container, Request("iface", FIXED_SEVEN))) == ["seven"]
    assert model.select(container, Request("iface", EIGHT)).status is Status.NOT_FOUND
    assert model.select(container, Request("iface", OTHER_TAG)).status is Status.NOT_FOUND
    assert model.select(container, Request("other")).status is Status.NOT_FOUND


def test_a_second_binding_of_a_key_declared_one_is_rejected(model):
    container = level(model, Binding("first", ("iface",), SEVEN, static=True))
    rejected = model.register(container, Binding("second", ("iface",), FIXED_SEVEN))
    assert rejected == (False, "lookup already registered: iface")
    assert model.register(container, Binding("eight", ("iface",), EIGHT)) == (True, "")


def test_a_key_declared_many_holds_any_number(model):
    container = level(
        model,
        Binding("first", ("iface",), static=True),
        Binding("second", ("iface",)),
        many=[("iface", NO_KEY)],
    )
    assert model.select(container, Request("iface")).status is Status.AMBIGUOUS
    collected = model.select(container, Request("iface"), collection=True)
    assert names(collected) == ["second", "first"]


def test_a_child_shadows_its_parent(model):
    parent = level(model, Binding("parent", ("iface",)))
    child = level(model, Binding("child", ("iface",)), parent=parent)
    empty_child = level(model, parent=parent)
    assert names(model.select(child, Request("iface"))) == ["child"]
    assert names(model.select(empty_child, Request("iface"))) == ["parent"]
    ambiguous = level(
        model,
        Binding("a", ("iface",)),
        Binding("b", ("iface",)),
        parent=parent,
        many=[("iface", NO_KEY)],
    )
    assert model.select(ambiguous, Request("iface")).status is Status.AMBIGUOUS


def test_local_bindings_shadow_the_container(model):
    container = level(model, Binding("container", ("iface",)))
    local = [Binding("local", ("iface",))]
    assert names(model.select(container, Request("iface"), local)) == ["local"]
    collected = model.select(container, Request("iface"), local, collection=True)
    assert names(collected) == ["container", "local"]


def test_an_unregistered_type_is_auto_constructed_last(model):
    parent = level(model, Binding("parent", ("iface",)))
    child = level(model, parent=parent)
    constructible = Request("iface", constructible=True)
    assert names(model.select(child, constructible)) == ["parent"]
    alone = level(model)
    assert model.select(alone, constructible).auto_constructed
    assert model.select(alone, Request("iface")).status is Status.NOT_FOUND
    ambiguous = level(
        model, Binding("a", ("iface",)), Binding("b", ("iface",)), many=[("iface", NO_KEY)]
    )
    assert model.select(ambiguous, constructible).status is Status.AMBIGUOUS


def test_a_collection_without_bindings_is_empty(model):
    selection = model.select(level(model), Request("iface"), collection=True)
    assert selection.status is Status.OK and selection.bindings == ()


def test_a_collection_asks_the_parent_only_when_the_child_names_none(model):
    parent = level(model, Binding("parent", ("iface",)))
    child = level(model, Binding("child", ("iface",)), parent=parent)
    assert names(model.select(child, Request("iface"), collection=True)) == ["child"]
    empty_child = level(model, parent=parent)
    assert names(model.select(empty_child, Request("iface"), collection=True)) == ["parent"]


def universe():
    """Containers of up to three bindings of one interface with every key, in
    both domains, with and without a parent."""
    candidates = [
        Binding(f"b{index}", ("iface",), key, static)
        for index, (key, static) in enumerate(itertools.product(KEYS, (False, True)))
    ]
    for count in range(4):
        yield from itertools.combinations(candidates, count)


def test_selection_returns_only_bindings_the_request_names(model):
    for bindings in universe():
        container = Level(declarations={("iface", key.domain()): Cardinality.MANY for key in KEYS})
        for binding in bindings:
            model.register(container, binding)
        for key in KEYS:
            request = Request("iface", key)
            for collection in (False, True):
                selection = model.select(container, request, collection=collection)
                named = [b for b in container.bindings if b.key.names(key)]
                assert set(selection.bindings) <= set(named)
                if collection:
                    assert sorted(selection.bindings, key=id) == sorted(named, key=id)
                elif len(named) == 1:
                    assert selection.bindings == tuple(named)
                elif named:
                    assert selection.status is Status.AMBIGUOUS


def test_registration_rejects_exactly_the_duplicates_of_a_domain_of_one(model):
    for bindings in universe():
        container = Level()
        for binding in bindings:
            duplicate = any(b.key.names(binding.key) for b in container.bindings)
            assert model.register(container, binding)[0] is not duplicate
