"""Tests of the case ids of test/selection/extract.py and of their mapping to
the reference selection implementation in test/selection/conform.py."""

import importlib.util
import sys
from pathlib import Path

import pytest

sys.dont_write_bytecode = True


def load(name, path):
    """Loads a script of this directory under its own module name, so it does
    not clash with the scripts of test/routes."""
    found = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(found)
    sys.modules[name] = module
    found.loader.exec_module(module)
    return module


HERE = Path(__file__).resolve().parent
conform = load("selection_conform", HERE / "conform.py")
extract = sys.modules["selection_extract"]
Binding = extract.Binding

import spec  # noqa: E402
from spec.reference.selection import Model  # noqa: E402


@pytest.fixture(scope="module")
def model():
    return Model(spec.load_spec())


def test_every_case_id_round_trips():
    for group in extract.build_groups(extract.DOMAINS):
        for index in range(len(group.requests)):
            case = group.case(index)
            domain, setting, bindings, mode, key = extract.parse_case_id(case)
            assert (domain, setting, bindings) == (
                group.domain,
                group.setting,
                group.bindings,
            )
            assert f"{mode}:{key}" == group.requests[index]


def test_the_place_of_a_single_place_domain_is_implied():
    parsed = extract.parse_case_id("static/many/impl_0:fixed7+impl_1:tag_a/all:fixed7")
    assert parsed == (
        "static",
        "many",
        (Binding("impl_0", "fixed7", "static"), Binding("impl_1", "tag_a", "static")),
        "all",
        "fixed7",
    )


@pytest.mark.parametrize(
    "case",
    [
        "runtime/default/impl_0:tag_c/one:none",
        "mixed/default/impl_0:none:parent/one:none",
        "runtime/default/empty/some:none",
        "runtime/default/empty/make:tag_a",
    ],
)
def test_malformed_case_ids_are_rejected(case):
    with pytest.raises(ValueError):
        extract.parse_case_id(case)


def test_canonical_bindings_put_static_before_runtime():
    for bindings in extract.binding_sequences("mixed"):
        places = [b.place for b in bindings]
        assert places == sorted(places, key=extract.PLACES["mixed"].index)


def test_mapping_marks_static_bindings_and_parent_levels():
    case = "parent/many/impl_0:tag_a:parent+impl_1:tag_a:child/one:tag_a"
    level, request, mode, impls, placed = conform.to_reference(case)
    (parent, first), (child, second) = placed
    assert child is level and parent is level.parent
    assert not first.static and not second.static
    assert impls == {first: 0, second: 1}
    assert request.key == conform.KEYS["tag_a"] and mode == "one"
    assert ("iface", ("tag", "tag_a")) in level.declarations

    mixed = "mixed/default/impl_0:fixed7:static+impl_0:runtime7:runtime/all:fixed7"
    level, _, mode, _, placed = conform.to_reference(mixed)
    assert [b.static for _, b in placed] == [True, False]
    assert level.declarations == {} and level.parent is None and mode == "all"


def test_reference_outcomes(model):
    # A fixed and a runtime key of one value are one key of one domain.
    case = "mixed/default/impl_0:fixed7:static+impl_1:runtime7:runtime/one:runtime7"
    assert conform.expected(model, case) == (["ok", "rejected"], 0)
    many = "runtime/many/impl_0:runtime7+impl_1:fixed7/all:fixed7"
    assert conform.expected(model, many) == (["ok", "ok"], [0, 1])
    assert conform.expected(model, "runtime/many/impl_0:none+impl_1:none/one:none") == (
        ["ok", "ok"],
        "ambiguous",
    )
    shadowed = "parent/default/impl_0:none:parent+impl_1:none:child/one:none"
    assert conform.expected(model, shadowed) == (["ok", "ok"], 1)


def test_a_key_outside_the_bound_is_never_stored(model):
    # The dense array of the bounded setting stores the key 7 and not 8, of a
    # fixed or a runtime key alike.
    case = "runtime/bounded/impl_0:runtime7+impl_1:fixed8/one:runtime8"
    assert conform.expected(model, case) == (["ok", "out_of_range"], "not_found")
    assert conform.observed_registrations(
        {
            "registration": "throws:type_index_out_of_range_exception",
            "registrations": ["ok", "throws:type_index_out_of_range_exception"],
        }
    ) == ["ok", "out_of_range"]
    assert conform.same_registrations("compile_error", ["out_of_range"])


def test_a_registration_rejected_for_its_bound_is_its_own_difference(model):
    rows = [
        {
            "id": "runtime/bounded/impl_0:runtime7+impl_1:runtime8/one:runtime7",
            "registration": "throws:std",
            "registrations": ["ok", "throws:std"],
            "selection": "ok",
            "selected": 0,
        }
    ]
    [(case, field, have, want)] = conform.compare(rows, model)
    assert field == "out_of_range" and want == ["ok", "out_of_range"]


def test_construct_resolves_a_binding_of_its_type(model):
    # construct names the binding as a singular request does; the abstract
    # interface has no factory, so without a binding it is not found.
    inherited = "parent/default/impl_0:none:parent/make:none"
    assert conform.expected(model, inherited) == (["ok"], 0)
    assert conform.expected(model, "runtime/default/empty/make:none") == ([], "not_found")
    ambiguous = "runtime/many/impl_0:none+impl_1:none/make:none"
    assert conform.expected(model, ambiguous) == (["ok", "ok"], "ambiguous")
    keyed = "runtime/default/impl_0:tag_a/make:none"
    assert conform.expected(model, keyed) == (["ok"], "not_found")


def test_a_compile_error_matches_a_rejection_or_not_found():
    assert conform.same_selection("compile_error", "not_found")
    assert conform.same_selection("compile_error", "ambiguous")
    assert not conform.same_selection("compile_error", [])
    assert conform.same_registrations("compile_error", ["ok", "rejected"])
    assert not conform.same_registrations("compile_error", ["ok", "ok"])
    assert not conform.same_registrations("not_expressible", ["ok"])


def test_keys_of_an_interface_and_of_other_key_types(model):
    from spec.reference.selection import Binding, Key, Level, Request

    seven = Key("runtime", "int", 7)
    nine = Key("runtime", "int", 9)
    name = Key("runtime", "string", "main")
    level = Level()
    binding = Binding(
        "b",
        ("processor", "animal"),
        seven,
        interface_keys=(("animal", nine),),
        aliases=(name,),
    )
    assert model.register(level, binding)[0]
    assert model.select(level, Request("processor", seven)).bindings == (binding,)
    assert model.select(level, Request("animal", nine)).bindings == (binding,)
    assert model.select(level, Request("animal", seven)).bindings == ()
    assert model.select(level, Request("animal", name)).bindings == (binding,)
    clash = Binding("c", ("processor",), Key("runtime", "int", 1), aliases=(name,))
    assert not model.register(level, clash)[0]
