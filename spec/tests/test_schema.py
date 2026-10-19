"""Tests of the schema types."""

import dataclasses
from enum import Enum

import pytest

from spec import schema
from spec.schema import Capability, Entity, Spec
from spec.tests.fixtures import sample_spec

DATACLASSES = [
    value
    for value in vars(schema).values()
    if isinstance(value, type) and dataclasses.is_dataclass(value)
]
ENUMS = [
    value
    for value in vars(schema).values()
    if isinstance(value, type) and issubclass(value, Enum)
]


@pytest.mark.parametrize("cls", DATACLASSES, ids=lambda cls: cls.__name__)
def test_dataclasses_are_frozen_slotted_keyword_only(cls):
    assert cls.__dataclass_params__.frozen
    assert hasattr(cls, "__slots__")
    assert not hasattr(cls, "__dict__") or "__dict__" not in cls.__slots__
    assert all(field.kw_only for field in dataclasses.fields(cls))


@pytest.mark.parametrize("cls", ENUMS, ids=lambda cls: cls.__name__)
def test_enum_values_are_snake_case_strings(cls):
    from spec.validate import SNAKE_CASE

    for member in cls:
        assert isinstance(member.value, str)
        assert SNAKE_CASE.match(member.value), member


def test_every_entity_has_an_id():
    for cls in DATACLASSES:
        if issubclass(cls, Entity):
            assert "id" in {field.name for field in dataclasses.fields(cls)}
    assert "id" in {field.name for field in dataclasses.fields(Capability)}


def test_capability_interface():
    fields = {field.name: field for field in dataclasses.fields(Capability)}
    assert list(sorted(fields)) == [
        "builtin",
        "description",
        "feature_macro",
        "header",
        "id",
        "minimum",
    ]
    capability = Capability(id="x", description="d")
    assert (capability.feature_macro, capability.minimum) == (None, None)
    assert (capability.builtin, capability.header) == (None, None)
    with pytest.raises(TypeError):
        Capability("x", "d")


def test_instances_are_immutable():
    capability = Capability(id="x", description="d")
    with pytest.raises(dataclasses.FrozenInstanceError):
        capability.id = "y"
    with pytest.raises(AttributeError):
        capability.extra = 1


def test_sample_spec_is_hashable():
    assert hash(sample_spec()) == hash(sample_spec())
    assert sample_spec() == sample_spec()


def test_spec_sections():
    names = [field.name for field in dataclasses.fields(Spec)]
    assert names == [
        "layer_kinds",
        "scopes",
        "routes",
        "key_kinds",
        "decorations",
        "lookup_filters",
        "index_structures",
        "factory_kinds",
        "selection_rules",
        "graph_rules",
        "diagnostics",
        "binding_time_constraints",
        "variant_points",
        "contracts",
        "behavior_changes",
        "public_api",
        "cpp",
    ]


def test_all_lists_the_public_types():
    assert set(schema.__all__) == {
        name
        for name, value in vars(schema).items()
        if isinstance(value, type) and value.__module__ == schema.__name__
    }
