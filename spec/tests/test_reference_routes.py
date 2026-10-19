"""Properties and cases of the reference route implementation."""

import itertools

import pytest

import spec
from spec.reference.routes import (
    Leaf,
    Model,
    Request,
    RequestKind,
    Shape,
    layer,
    leaf,
    resolve,
)

COPYABLE = Leaf("leaf")
MOVE_ONLY = Leaf("move_only_leaf", copyable=False)
OTHER = Leaf("other_leaf")

WRAPPERS = ("own", "share", "opt")
SCOPES = ("external", "shared", "shared_cyclical", "unique")
KINDS = (RequestKind.VALUE, RequestKind.LVALUE_REFERENCE, RequestKind.RVALUE_REFERENCE)


@pytest.fixture(scope="module")
def model():
    return Model(spec.load_spec())


def stored_shapes(value):
    base = leaf(value)
    yield base
    yield layer("ptr", base)
    yield layer("ref", base)
    yield layer("array", base, extent=2)
    yield layer("alt", base, leaf(OTHER))
    for outer in WRAPPERS:
        yield layer(outer, base)
        yield layer(outer, layer("array_unbounded", base))
        for inner in WRAPPERS:
            yield layer(outer, layer(inner, base))


def request_shapes(stored):
    """The leaf, the standard wrappers of it, the stored shape and its layers,
    and pointers to each."""
    value = stored
    while value.layer is not None:
        value = value.args[0]
    shapes = {value, stored}
    for wrapper in WRAPPERS:
        shapes.add(layer(wrapper, value))
    inner = stored
    while inner.layer is not None:
        shapes.add(inner)
        inner = inner.args[0]
    return shapes | {layer("ptr", shape) for shape in shapes}


def cases():
    for value in (COPYABLE, MOVE_ONLY):
        for stored in stored_shapes(value):
            for shape in request_shapes(stored):
                for kind in KINDS:
                    yield stored, Request(kind, shape)


ALL_CASES = list(cases())


def outcomes(model, scope):
    for stored, request in ALL_CASES:
        yield stored, request, resolve(model, scope, stored, request)


def test_the_universe_is_large(model):
    assert len(ALL_CASES) > 1000


@pytest.mark.parametrize("scope", ["external", "shared", "shared_cyclical"])
def test_borrowing_never_serves_rvalue_references(model, scope):
    for stored, request, outcome in outcomes(model, scope):
        if request.kind == RequestKind.RVALUE_REFERENCE:
            assert not outcome.served, (stored, request)


def test_consuming_never_serves_lvalue_references(model):
    for stored, request, outcome in outcomes(model, "unique"):
        if request.kind == RequestKind.LVALUE_REFERENCE:
            assert not outcome.served, (stored, request)


@pytest.mark.parametrize("scope", SCOPES)
def test_an_unregistered_binding_serves_nothing(model, scope):
    for stored, request, outcome in outcomes(model, scope):
        if not outcome.registered:
            assert not outcome.served, (stored, request)


@pytest.mark.parametrize("scope", ["external", "shared", "shared_cyclical"])
def test_borrowed_references_refer_to_the_stored_object(model, scope):
    for stored, request, outcome in outcomes(model, scope):
        if outcome.served and request.kind == RequestKind.LVALUE_REFERENCE:
            assert outcome.copies == 0, (stored, request)
            assert outcome.stable, (stored, request)
            assert outcome.identity[1] == "existing", (stored, request)


def test_consumed_results_are_new(model):
    for stored, request, outcome in outcomes(model, "unique"):
        if outcome.served:
            assert not outcome.stable, (stored, request)
            assert outcome.copies == 0, (stored, request)
            assert outcome.identity in ((None, None), ("new", "new")), (stored, request)


def test_shared_cyclical_never_copies_a_leaf(model):
    for stored, request, outcome in outcomes(model, "shared_cyclical"):
        assert outcome.copies == 0, (stored, request)


def test_move_only_leaves_are_never_copied(model):
    for scope in SCOPES:
        for stored, request, outcome in outcomes(model, scope):
            if outcome.served and "move_only_leaf" in repr(stored):
                assert outcome.copies == 0, (scope, stored, request)


def test_arrays_are_never_handed_out_as_values(model):
    for scope in SCOPES:
        for stored, request, outcome in outcomes(model, scope):
            if request.shape.layer in ("array", "array_unbounded"):
                if request.kind != RequestKind.LVALUE_REFERENCE:
                    assert not outcome.served, (scope, stored, request)


def test_the_stored_pointer_is_never_referred_to(model):
    for scope in SCOPES:
        for stored, request, outcome in outcomes(model, scope):
            if request.shape.layer == "ptr" and request.kind == RequestKind.LVALUE_REFERENCE:
                assert not outcome.served, (scope, stored, request)


def test_constructions_follow_the_slot(model):
    stored = leaf(COPYABLE)
    request = Request(RequestKind.VALUE, stored)
    assert resolve(model, "external", stored, request).constructions == (0, 0)
    assert resolve(model, "shared", stored, request).constructions == (1, 0)
    assert resolve(model, "unique", stored, request).constructions == (1, 1)


CASES = [
    # scope, stored, request kind, request shape, served, rules
    ("shared", leaf(COPYABLE), RequestKind.VALUE, leaf(COPYABLE), True, ("copy",)),
    ("shared", leaf(MOVE_ONLY), RequestKind.VALUE, leaf(MOVE_ONLY), False, ()),
    ("unique", leaf(COPYABLE), RequestKind.LVALUE_REFERENCE, leaf(COPYABLE), False, ()),
    (
        "unique",
        layer("own", leaf(COPYABLE)),
        RequestKind.VALUE,
        layer("share", leaf(COPYABLE)),
        True,
        ("share_owned",),
    ),
    (
        "unique",
        layer("array", leaf(COPYABLE), extent=2),
        RequestKind.VALUE,
        layer("share", leaf(COPYABLE)),
        False,
        (),
    ),
    (
        "shared",
        layer("share", layer("opt", leaf(COPYABLE))),
        RequestKind.LVALUE_REFERENCE,
        leaf(COPYABLE),
        True,
        ("borrow_shared", "borrow_optional"),
    ),
    (
        "shared",
        layer("array", leaf(COPYABLE), extent=2),
        RequestKind.VALUE,
        layer("ptr", leaf(COPYABLE)),
        True,
        ("decay_array",),
    ),
]


@pytest.mark.parametrize("scope, stored, kind, shape, served, rules", CASES)
def test_cases(model, scope, stored, kind, shape, served, rules):
    outcome = resolve(model, scope, stored, Request(kind, shape))
    assert outcome.served is served
    assert outcome.rules == rules


@pytest.mark.parametrize(
    "scope, stored, registered",
    [
        ("shared_cyclical", layer("share", layer("share", leaf(COPYABLE))), False),
        ("shared_cyclical", layer("ptr", leaf(COPYABLE)), False),
        ("shared_cyclical", layer("share", leaf(COPYABLE)), True),
        ("shared", layer("ref", leaf(COPYABLE)), False),
        ("external", layer("ref", leaf(COPYABLE)), True),
        ("shared", layer("share", layer("array_unbounded", leaf(COPYABLE))), False),
        ("external", layer("share", layer("array_unbounded", leaf(COPYABLE))), True),
    ],
)
def test_registration(model, scope, stored, registered):
    outcome = resolve(model, scope, stored, Request(RequestKind.VALUE, stored))
    assert outcome.registered is registered
