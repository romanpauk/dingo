"""Tests of spec validation: the sample is clean and each error kind is reported."""

import dataclasses
from dataclasses import replace

import pytest

from spec import SpecError, build_spec, load_spec, validate
from spec.schema import (
    BindingTime,
    Bound,
    BoundKind,
    Capability,
    Cardinality,
    Combine,
    Contract,
    ContractDimension,
    CppApiBinding,
    Diagnostic,
    PatternForm,
    Requirement,
    Predicate,
    ShapePattern,
    SelectionMode,
    SelectionOutcome,
    SelectionSource,
    SelectionStep,
    Status,
    StepCondition,
    VariantKind,
    VariantPoint,
)
from spec.tests.fixtures import layer, sample_spec, variable


def errors_of(spec):
    return validate(spec)


def replace_first(spec, section, **changes):
    items = getattr(spec, section)
    return replace(spec, **{section: (replace(items[0], **changes), *items[1:])})


def assert_one_error(spec, fragment):
    errors = errors_of(spec)
    assert any(fragment in error for error in errors), errors
    return errors


def test_sample_is_valid():
    assert errors_of(sample_spec()) == []


def test_shipped_spec_is_valid():
    assert errors_of(build_spec()) == []


def test_load_spec_raises_with_all_errors(monkeypatch):
    import spec as package

    bad = replace_first(sample_spec(), "scopes", id="Bad")
    bad = replace_first(bad, "layer_kinds", id="Worse")
    monkeypatch.setattr(package, "build_spec", lambda: bad)
    with pytest.raises(SpecError) as raised:
        package.load_spec()
    assert "scopes: id 'Bad' is not snake_case" in str(raised.value)
    assert "layer_kinds: id 'Worse' is not snake_case" in str(raised.value)


@pytest.mark.parametrize("bad", ["Upper", "has-dash", "_lead", "trail_", "dou__ble", "1digit", ""])
def test_ids_must_be_snake_case(bad):
    spec = replace_first(sample_spec(), "scopes", id=bad)
    assert_one_error(spec, "is not snake_case")


def test_ids_must_be_unique_within_a_section():
    spec = sample_spec()
    duplicate = replace(spec, layer_kinds=(*spec.layer_kinds, spec.layer_kinds[0]))
    assert_one_error(duplicate, "layer_kinds: duplicate id 'val'")


def test_ids_may_repeat_across_sections():
    spec = sample_spec()
    same = replace_first(spec, "decorations", id="shared")
    assert errors_of(same) == []


def test_capability_ids_are_checked():
    spec = sample_spec()
    capabilities = (*spec.cpp.capabilities, spec.cpp.capabilities[0])
    spec = replace(spec, cpp=replace(spec.cpp, capabilities=capabilities))
    assert_one_error(spec, "cpp.capabilities: duplicate id")


def test_scope_extends_must_exist():
    spec = replace_first(sample_spec(), "scopes", extends="missing")
    assert_one_error(spec, "extends scope 'missing' does not exist")


def test_scope_extends_cycle():
    spec = sample_spec()
    scopes = (
        replace(spec.scopes[0], extends="shared_cyclical"),
        spec.scopes[1],
    )
    assert_one_error(replace(spec, scopes=scopes), "extends cycle")


def test_scope_phases_and_needs_are_snake_case_and_unique():
    spec = replace_first(sample_spec(), "scopes", phases=("Publish", "Publish"))
    errors = errors_of(spec)
    assert any("phases has duplicates" in error for error in errors)
    assert any("phases entry 'Publish' is not snake_case" in error for error in errors)


def test_layer_kind_parameters():
    spec = replace_first(sample_spec(), "layer_kinds", parameters=-1)
    assert_one_error(spec, "parameters is negative")
    spec = replace_first(sample_spec(), "layer_kinds", variadic=True)
    assert_one_error(spec, "variadic needs at least one parameter")


def test_layer_kind_traits_unique():
    from spec.schema import LayerTrait

    spec = replace_first(
        sample_spec(), "layer_kinds", traits=(LayerTrait.OWNING, LayerTrait.OWNING)
    )
    assert_one_error(spec, "traits has duplicates")


def test_route_unknown_layer_kind():
    spec = replace_first(sample_spec(), "routes", to_shape=layer("missing", variable("X")))
    assert_one_error(spec, "layer kind 'missing' does not exist")


def test_route_layer_argument_count():
    spec = replace_first(sample_spec(), "routes", to_shape=layer("ptr"))
    assert_one_error(spec, "ptr takes 1 arguments")
    spec = replace_first(sample_spec(), "routes", to_shape=layer("val", variable("X")))
    assert_one_error(spec, "val takes 0 arguments")


def test_route_variadic_layer_needs_minimum_arguments():
    ok = replace_first(
        sample_spec(), "routes", to_shape=layer("alt", variable("X"), variable("Y"))
    )
    assert errors_of(ok) == []
    bad = replace_first(sample_spec(), "routes", to_shape=layer("alt"))
    assert_one_error(bad, "alt takes at least 1 arguments")


def test_route_pattern_forms_are_well_formed():
    spec = replace_first(
        sample_spec(),
        "routes",
        from_shape=ShapePattern(form=PatternForm.VARIABLE, name="lower"),
    )
    assert_one_error(spec, "pattern variable 'lower' is malformed")
    spec = replace_first(
        sample_spec(),
        "routes",
        from_shape=ShapePattern(form=PatternForm.WRAPPER, name="W"),
    )
    assert_one_error(spec, "wrapper 'W' takes one argument")
    spec = replace_first(
        sample_spec(), "routes", to_shape=ShapePattern(form=PatternForm.ROUTE)
    )
    assert_one_error(spec, "route pattern takes one argument")


def test_route_requirement_subject_must_be_bound():
    spec = replace_first(
        sample_spec(),
        "routes",
        requires=(Requirement(predicate=Predicate.COPYABLE, subject="Z"),),
    )
    assert_one_error(spec, "requirement subject 'Z' is not a pattern variable")


def test_route_cache_pattern_is_checked():
    spec = replace_first(sample_spec(), "routes", cache=layer("missing"))
    assert_one_error(spec, "layer kind 'missing' does not exist")


def test_lookup_filter_dimensions():
    spec = replace_first(sample_spec(), "lookup_filters", dimensions=())
    assert_one_error(spec, "no dimensions")


def test_index_structure_references():
    spec = replace_first(sample_spec(), "index_structures", filter="missing")
    assert_one_error(spec, "filter 'missing' does not exist")
    spec = replace_first(sample_spec(), "index_structures", key_kinds=("missing",))
    assert_one_error(spec, "key kind 'missing' does not exist")
    spec = replace_first(sample_spec(), "index_structures", key_kinds=())
    assert_one_error(spec, "no key kinds")
    spec = replace_first(sample_spec(), "index_structures", declarations=("missing",))
    assert_one_error(spec, "declaration 'missing' does not exist")


def test_lookup_declarations_are_checked():
    spec = replace_first(sample_spec(), "lookup_declarations", default=False)
    assert_one_error(spec, "exactly one is the default")
    spec = replace_first(sample_spec(), "lookup_declarations", key_kinds=("missing",))
    assert_one_error(spec, "key kind 'missing' does not exist")
    spec = replace_first(
        sample_spec(), "lookup_declarations", cardinalities=(Cardinality.MANY,)
    )
    assert_one_error(spec, "the default cardinality is not allowed")
    spec = replace_first(
        sample_spec(),
        "lookup_declarations",
        cardinalities=(Cardinality.EMPTY, Cardinality.ONE),
    )
    assert_one_error(spec, "cardinalities are one or many")


def test_selection_rule_shape():
    spec = replace_first(sample_spec(), "selection_rules", steps=())
    assert_one_error(spec, "no steps")
    spec = replace_first(sample_spec(), "selection_rules", combine=Combine.CONCAT)
    assert_one_error(spec, "singular requests combine with first_non_empty")
    spec = replace_first(
        sample_spec(),
        "selection_rules",
        outcomes=(SelectionOutcome(count=Cardinality.ONE, status=Status.OK),),
    )
    assert_one_error(spec, "outcomes must cover empty, one, and many")


def test_a_factory_is_a_conditional_source_of_construct_requests():
    factory = SelectionStep(sources=(SelectionSource.FACTORY,), when=StepCondition.HAS_FACTORY)
    spec = replace_first(sample_spec(), "selection_rules", steps=(factory,))
    assert_one_error(spec, "a factory is a source of construct requests")
    spec = replace_first(
        sample_spec(),
        "selection_rules",
        mode=SelectionMode.CONSTRUCT,
        steps=(SelectionStep(sources=(SelectionSource.FACTORY,)),),
    )
    assert_one_error(spec, "a factory is a source of construct requests")
    spec = replace_first(
        sample_spec(), "selection_rules", mode=SelectionMode.CONSTRUCT, steps=(factory,)
    )
    validate(spec)


def test_graph_rule_references():
    spec = replace_first(sample_spec(), "graph_rules", legal_slots=())
    assert_one_error(spec, "no legal slots")
    spec = replace_first(sample_spec(), "graph_rules", diagnostic="missing")
    assert_one_error(spec, "diagnostic 'missing' does not exist")
    spec = replace_first(sample_spec(), "graph_rules", checked_at=())
    assert_one_error(spec, "no check times")


def test_diagnostic_placeholders_must_match_message():
    spec = replace_first(sample_spec(), "diagnostics", placeholders=("request", "count"))
    assert_one_error(spec, "undeclared placeholder 'candidates'")
    spec = replace_first(
        sample_spec(),
        "diagnostics",
        placeholders=("request", "count", "candidates", "extra"),
    )
    assert_one_error(spec, "placeholder 'extra' is not used")


def test_diagnostic_message_must_be_a_valid_template():
    spec = replace_first(sample_spec(), "diagnostics", message="bad {request", placeholders=())
    assert_one_error(spec, "message is malformed")
    spec = replace_first(
        sample_spec(), "diagnostics", message="{count:>5}", placeholders=("count",)
    )
    assert_one_error(spec, "conversion or format")


def test_diagnostic_fields_are_not_empty():
    spec = replace_first(sample_spec(), "diagnostics", condition=" ")
    assert_one_error(spec, "condition is empty")


def test_a_runtime_diagnostic_has_an_exception():
    spec = replace_first(sample_spec(), "diagnostics", exception=" ")
    assert_one_error(spec, "has an exception if and only if it is reported at runtime")


def test_a_compiled_diagnostic_has_an_example():
    spec = replace_first(
        sample_spec(),
        "diagnostics",
        reported=(BindingTime.COMPILE, BindingTime.RUNTIME),
    )
    assert_one_error(spec, "with no cpp.diagnostic_examples entry")


def test_diagnostic_placeholder_names():
    spec = replace_first(
        sample_spec(), "diagnostics", message="{Bad}", placeholders=("Bad",)
    )
    assert_one_error(spec, "placeholder 'Bad' is not snake_case")


def test_binding_time_constraint_needs_justification():
    spec = replace_first(sample_spec(), "binding_time_constraints", justification="")
    assert_one_error(spec, "justification is empty")


def test_variant_point_references():
    spec = replace_first(sample_spec(), "variant_points", capability="missing")
    assert_one_error(spec, "capability 'missing' does not exist")
    base = sample_spec()
    points = (
        base.variant_points[0],
        replace(base.variant_points[1], index_structure="missing"),
        base.variant_points[2],
    )
    assert_one_error(replace(base, variant_points=points), "index structure 'missing' does not exist")


def test_variant_point_kind_fields():
    spec = replace_first(sample_spec(), "variant_points", capability=None)
    assert_one_error(spec, "a language tier names a capability")
    extra = VariantPoint(
        id="extra",
        kind=VariantKind.COMPILER_VARIANT,
        justification="x",
        capability="pack_indexing",
    )
    spec = sample_spec()
    assert_one_error(
        replace(spec, variant_points=(*spec.variant_points, extra)),
        "only a language tier names a capability",
    )
    index_less = VariantPoint(id="idx", kind=VariantKind.INDEX_STRUCTURE, justification="x")
    assert_one_error(
        replace(spec, variant_points=(*spec.variant_points, index_less)),
        "an index structure variant names an index structure",
    )
    stray = VariantPoint(
        id="stray",
        kind=VariantKind.BINDING_TIME_DISPATCH,
        justification="x",
        index_structure="ordered",
    )
    assert_one_error(
        replace(spec, variant_points=(*spec.variant_points, stray)),
        "only an index structure variant names an index structure",
    )


def test_variant_point_justification():
    spec = replace_first(sample_spec(), "variant_points", justification="")
    assert_one_error(spec, "justification is empty")


@pytest.mark.parametrize(
    "bound, fragment",
    [
        (Bound(kind=BoundKind.AT_MOST), "at_most needs a non-negative value"),
        (Bound(kind=BoundKind.EXACTLY, value=-1), "exactly needs a non-negative value"),
        (Bound(kind=BoundKind.CONSTANT, value=1), "constant takes no value"),
        (Bound(kind=BoundKind.LINEAR), "linear names its variable"),
        (Bound(kind=BoundKind.AT_MOST, value=1, variable="n"), "at_most takes no variable"),
        (Bound(kind=BoundKind.CONSTANT, unit="bytes"), "constant takes no unit"),
    ],
)
def test_contract_bounds(bound, fragment):
    spec = replace_first(sample_spec(), "contracts", bound=bound)
    assert_one_error(spec, fragment)


def test_contract_bound_must_fit_dimension():
    spec = replace_first(sample_spec(), "contracts", bound=Bound(kind=BoundKind.ALWAYS))
    assert_one_error(spec, "always does not fit runtime_cost")
    base = sample_spec()
    inline = replace(
        base.contracts[1], bound=Bound(kind=BoundKind.AT_MOST, value=1)
    )
    spec = replace(base, contracts=(base.contracts[0], inline, base.contracts[2]))
    assert_one_error(spec, "at_most does not fit inlining")


def test_contract_operation_is_not_empty():
    spec = replace_first(sample_spec(), "contracts", operation="")
    assert_one_error(spec, "operation is empty")


def test_behavior_change_fields():
    spec = replace_first(sample_spec(), "behavior_changes", cases=())
    assert_one_error(spec, "cases is not a non-empty list")
    spec = replace_first(sample_spec(), "behavior_changes", decision="")
    assert_one_error(spec, "decision is empty")


def test_cpp_api_binding_references():
    spec = sample_spec()
    binding = replace(spec.cpp.api_bindings[0], api="missing")
    spec = replace(spec, cpp=replace(spec.cpp, api_bindings=(binding,)))
    assert_one_error(spec, "public API entry 'missing' does not exist")


def test_cpp_api_binding_is_unique_per_entry():
    spec = sample_spec()
    second = CppApiBinding(id="again", api="container_header", spelling="x")
    bindings = (*spec.cpp.api_bindings, second)
    spec = replace(spec, cpp=replace(spec.cpp, api_bindings=bindings))
    assert_one_error(spec, "public API entries has duplicates")


def with_capability(**fields):
    spec = sample_spec()
    capability = Capability(id="pack_indexing", description="Probe.", **fields)
    return replace(spec, cpp=replace(spec.cpp, capabilities=(capability,)))


@pytest.mark.parametrize(
    "fields, fragment",
    [
        ({"feature_macro": "__cpp_x"}, "feature_macro and minimum are set together"),
        ({"minimum": 1}, "feature_macro and minimum are set together"),
        ({}, "no detection"),
        (
            {"feature_macro": "__cpp_x", "minimum": 1, "builtin": "__b"},
            "not both",
        ),
        ({"builtin": "__b", "minimum": 1}, "feature_macro and minimum are set together"),
        ({"feature_macro": "1bad", "minimum": 1}, "feature_macro '1bad' is malformed"),
        ({"feature_macro": "__cpp_x", "minimum": 0}, "minimum must be a positive integer"),
        ({"feature_macro": "__cpp_x", "minimum": True}, "minimum must be a positive integer"),
        ({"builtin": "has space"}, "builtin 'has space' is malformed"),
        ({"builtin": "__b", "header": "version"}, "header needs a feature_macro"),
        (
            {"feature_macro": "__cpp_x", "minimum": 1, "header": "bad header"},
            "header 'bad header' is malformed",
        ),
    ],
)
def test_capability_detection_errors(fields, fragment):
    assert_one_error(with_capability(**fields), fragment)


@pytest.mark.parametrize(
    "fields",
    [
        {"feature_macro": "__cpp_x", "minimum": 202311},
        {"feature_macro": "__cpp_lib_x", "minimum": 202110, "header": "version"},
        {"builtin": "__type_pack_element"},
    ],
)
def test_valid_capability_detection(fields):
    assert errors_of(with_capability(**fields)) == []


def test_capability_description_is_not_empty():
    spec = sample_spec()
    capability = replace(spec.cpp.capabilities[0], description=" ")
    spec = replace(spec, cpp=replace(spec.cpp, capabilities=(capability,)))
    assert_one_error(spec, "description is empty")


def test_every_section_is_checked_for_ids():
    """Each section of the Spec is covered by the id check."""
    from spec.validate import sections

    names = [name for name, _ in sections(sample_spec())]
    expected = [f.name for f in dataclasses.fields(type(sample_spec())) if f.name != "cpp"]
    expected += [f"cpp.{f.name}" for f in dataclasses.fields(type(sample_spec().cpp))]
    assert names == expected


def test_transaction_rule_shape():
    spec = replace_first(sample_spec(), "transaction_rules", undoes=())
    assert_one_error(spec, "undoes no effect")
    spec = replace_first(sample_spec(), "transaction_rules", operations=())
    assert_one_error(spec, "no operations")
