"""Properties and cases of the reference route implementation."""

import itertools

import pytest

import spec
from spec.reference.routes import (
    Leaf,
    Model,
    Request,
    RequestForm,
    TypeTree,
    wrapper,
    leaf,
    resolve,
)

COPYABLE = Leaf("leaf")
MOVE_ONLY = Leaf("move_only_leaf", copyable=False)
OTHER = Leaf("other_leaf")

WRAPPERS = ("own", "share", "opt")
SCOPES = ("external", "shared", "shared_cyclical", "unique")
KINDS = (RequestForm.VALUE, RequestForm.LVALUE_REFERENCE, RequestForm.RVALUE_REFERENCE)


@pytest.fixture(scope="module")
def model():
    return Model(spec.load_spec())


def stored_type_trees(value):
    base = leaf(value)
    yield base
    yield wrapper("ptr", base)
    yield wrapper("ref", base)
    yield wrapper("array", base, extent=2)
    yield wrapper("alt", base, leaf(OTHER))
    for outer in WRAPPERS:
        yield wrapper(outer, base)
        yield wrapper(outer, wrapper("array_unbounded", base))
        for inner in WRAPPERS:
            yield wrapper(outer, wrapper(inner, base))


def request_type_trees(stored):
    """The leaf, the standard wrappers of it, the stored type tree and its wrappers,
    and pointers to each."""
    value = stored
    while value.wrapper is not None:
        value = value.args[0]
    type_trees = {value, stored}
    for semantics in WRAPPERS:
        type_trees.add(wrapper(semantics, value))
    inner = stored
    while inner.wrapper is not None:
        type_trees.add(inner)
        inner = inner.args[0]
    return type_trees | {wrapper("ptr", type_tree) for type_tree in type_trees}


def cases():
    for value in (COPYABLE, MOVE_ONLY):
        for stored in stored_type_trees(value):
            for type_tree in request_type_trees(stored):
                for kind in KINDS:
                    yield stored, Request(kind, type_tree)


ALL_CASES = list(cases())


def outcomes(model, scope):
    for stored, request in ALL_CASES:
        yield stored, request, resolve(model, scope, stored, request)


def test_the_universe_is_large(model):
    assert len(ALL_CASES) > 1000


@pytest.mark.parametrize("scope", ["external", "shared", "shared_cyclical"])
def test_borrowing_never_serves_rvalue_references(model, scope):
    for stored, request, outcome in outcomes(model, scope):
        if request.kind == RequestForm.RVALUE_REFERENCE:
            assert not outcome.served, (stored, request)


def test_consuming_never_serves_lvalue_references(model):
    for stored, request, outcome in outcomes(model, "unique"):
        if request.kind == RequestForm.LVALUE_REFERENCE:
            assert not outcome.served, (stored, request)


@pytest.mark.parametrize("scope", SCOPES)
def test_an_unregistered_binding_serves_nothing(model, scope):
    for stored, request, outcome in outcomes(model, scope):
        if not outcome.registered:
            assert not outcome.served, (stored, request)


@pytest.mark.parametrize("scope", ["external", "shared", "shared_cyclical"])
def test_borrowed_references_refer_to_the_stored_object(model, scope):
    for stored, request, outcome in outcomes(model, scope):
        if outcome.served and request.kind == RequestForm.LVALUE_REFERENCE:
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
            if request.type_tree.wrapper in ("array", "array_unbounded"):
                if request.kind != RequestForm.LVALUE_REFERENCE:
                    assert not outcome.served, (scope, stored, request)


def test_the_stored_pointer_is_never_referred_to(model):
    for scope in SCOPES:
        for stored, request, outcome in outcomes(model, scope):
            if request.type_tree.wrapper == "ptr" and request.kind == RequestForm.LVALUE_REFERENCE:
                assert not outcome.served, (scope, stored, request)


def test_constructions_follow_the_slot(model):
    stored = leaf(COPYABLE)
    request = Request(RequestForm.VALUE, stored)
    assert resolve(model, "external", stored, request).constructions == (0, 0)
    assert resolve(model, "shared", stored, request).constructions == (1, 0)
    assert resolve(model, "unique", stored, request).constructions == (1, 1)


CASES = [
    # scope, stored, request kind, request type tree, served, rules
    ("shared", leaf(COPYABLE), RequestForm.VALUE, leaf(COPYABLE), True, ("copy",)),
    ("shared", leaf(MOVE_ONLY), RequestForm.VALUE, leaf(MOVE_ONLY), False, ()),
    ("unique", leaf(COPYABLE), RequestForm.LVALUE_REFERENCE, leaf(COPYABLE), False, ()),
    (
        "unique",
        wrapper("own", leaf(COPYABLE)),
        RequestForm.VALUE,
        wrapper("share", leaf(COPYABLE)),
        True,
        ("share_owned",),
    ),
    (
        "unique",
        wrapper("array", leaf(COPYABLE), extent=2),
        RequestForm.VALUE,
        wrapper("share", leaf(COPYABLE)),
        False,
        (),
    ),
    (
        "shared",
        wrapper("share", wrapper("opt", leaf(COPYABLE))),
        RequestForm.LVALUE_REFERENCE,
        leaf(COPYABLE),
        True,
        ("borrow_shared", "borrow_optional"),
    ),
    (
        "shared",
        wrapper("array", leaf(COPYABLE), extent=2),
        RequestForm.VALUE,
        wrapper("ptr", leaf(COPYABLE)),
        True,
        ("decay_array",),
    ),
]


@pytest.mark.parametrize("scope, stored, kind, type_tree, served, rules", CASES)
def test_cases(model, scope, stored, kind, type_tree, served, rules):
    outcome = resolve(model, scope, stored, Request(kind, type_tree))
    assert outcome.served is served
    assert outcome.rules == rules


@pytest.mark.parametrize(
    "scope, stored, registered",
    [
        ("shared_cyclical", wrapper("share", wrapper("share", leaf(COPYABLE))), False),
        ("shared_cyclical", wrapper("ptr", leaf(COPYABLE)), False),
        ("shared_cyclical", wrapper("share", leaf(COPYABLE)), True),
        ("shared", wrapper("ref", leaf(COPYABLE)), False),
        ("external", wrapper("ref", leaf(COPYABLE)), True),
        ("shared", wrapper("share", wrapper("array_unbounded", leaf(COPYABLE))), False),
        ("external", wrapper("share", wrapper("array_unbounded", leaf(COPYABLE))), True),
    ],
)
def test_registration(model, scope, stored, registered):
    outcome = resolve(model, scope, stored, Request(RequestForm.VALUE, stored))
    assert outcome.registered is registered


IMPL = Leaf("impl", bases=("iface", "other_iface"))
IFACE = Leaf("iface", copyable=False, constructible=False)
OTHER_IFACE = Leaf("other_iface", copyable=False, constructible=False)


def test_a_binding_with_interfaces_is_found_only_by_them(model):
    stored = wrapper("share", leaf(IMPL))
    by_impl = resolve(model, "shared", stored, Request(RequestForm.LVALUE_REFERENCE, leaf(IMPL)), (IFACE,))
    by_iface = resolve(model, "shared", stored, Request(RequestForm.LVALUE_REFERENCE, leaf(IFACE)), (IFACE,))
    assert not by_impl.found and not by_impl.served
    assert by_iface.served and by_iface.rules == ("borrow_shared", "upcast_reference")


def test_an_undeclared_interface_is_not_found(model):
    stored = leaf(IMPL)
    request = Request(RequestForm.LVALUE_REFERENCE, leaf(OTHER_IFACE))
    assert not resolve(model, "shared", stored, request, (IFACE,)).found


@pytest.mark.parametrize(
    "stored, request_type_tree, rules",
    [
        (wrapper("share", leaf(IMPL)), wrapper("share", leaf(IFACE)), ("copy", "upcast_copied_shared")),
        (wrapper("ptr", leaf(IMPL)), wrapper("ptr", leaf(IFACE)), ("copy", "upcast_pointer")),
    ],
)
def test_borrowed_interfaces_convert(model, stored, request_type_tree, rules):
    outcome = resolve(model, "shared", stored, Request(RequestForm.VALUE, request_type_tree), (IFACE,))
    assert outcome.served and outcome.rules == rules


def test_a_shared_interface_handle_is_retained(model):
    stored = wrapper("share", leaf(IMPL))
    request = Request(RequestForm.LVALUE_REFERENCE, wrapper("share", leaf(IFACE)))
    outcome = resolve(model, "shared", stored, request, (IFACE, OTHER_IFACE))
    assert outcome.served and outcome.rules == ("retain_shared_upcast",)
    assert outcome.stable


def test_consumed_interfaces_convert(model):
    stored = wrapper("own", leaf(IMPL))
    request = Request(RequestForm.VALUE, wrapper("share", leaf(IFACE)))
    outcome = resolve(model, "unique", stored, request, (IFACE,))
    assert outcome.served


@pytest.mark.parametrize(
    "scope, stored, registered",
    [
        ("unique", leaf(IMPL), False),
        ("shared", wrapper("array", leaf(IMPL), extent=2), False),
        ("shared", wrapper("opt", leaf(IMPL)), True),
        ("unique", wrapper("share", wrapper("share", leaf(IMPL))), False),
        ("unique", wrapper("own", leaf(IMPL)), True),
    ],
)
def test_interface_registration(model, scope, stored, registered):
    outcome = resolve(model, scope, stored, Request(RequestForm.VALUE, stored), (IFACE,))
    assert outcome.registered is registered


def test_a_converted_handle_keeps_its_family(model):
    stored = wrapper("own", leaf(COPYABLE), family="user")
    user = Request(RequestForm.VALUE, wrapper("share", leaf(COPYABLE), family="user"))
    std = Request(RequestForm.VALUE, wrapper("share", leaf(COPYABLE), family="std"))
    assert resolve(model, "unique", stored, user).served
    assert not resolve(model, "unique", stored, std).served


def test_a_created_wrapper_may_be_of_any_family(model):
    stored = leaf(COPYABLE)
    for family in ("std", "user"):
        request = Request(RequestForm.VALUE, wrapper("opt", leaf(COPYABLE), family=family))
        assert resolve(model, "unique", stored, request).served


ABSTRACT = Leaf("abstract", constructible=False)


@pytest.mark.parametrize("scope", ("shared", "shared_cyclical", "unique"))
def test_a_declared_factory_makes_an_abstract_leaf(model, scope):
    from spec.reference.routes import register

    stored = leaf(ABSTRACT) if scope == "shared_cyclical" else wrapper("own", leaf(ABSTRACT))
    if scope == "unique":
        stored = wrapper("ptr", leaf(ABSTRACT))
    assert register(model, scope, stored) == (False, "the stored type tree has no default factory")
    assert register(model, scope, stored, factory="function") == (True, "")


def test_an_unknown_factory_kind_is_rejected(model):
    from spec.reference.routes import register

    with pytest.raises(ValueError):
        register(model, "shared", leaf(COPYABLE), factory="missing")


@pytest.mark.parametrize("scope", SCOPES)
def test_a_variant_names_every_alternative(model, scope):
    from spec.reference.routes import ALTERNATIVE_NOT_HELD

    stored = wrapper("alt", leaf(COPYABLE), leaf(OTHER))
    if scope == "external":
        stored = wrapper("ref", stored)
    kind = RequestForm.VALUE if scope == "unique" else RequestForm.LVALUE_REFERENCE
    held = resolve(model, scope, stored, Request(kind, leaf(COPYABLE)))
    assert held.served
    other = resolve(model, scope, stored, Request(kind, leaf(OTHER)))
    assert other.found and not other.served
    assert other.reason == ALTERNATIVE_NOT_HELD
    absent = resolve(model, scope, stored, Request(kind, leaf(MOVE_ONLY)))
    assert absent.registered and not absent.found
