"""Tests of the shipped spec and the package interface."""

import dataclasses

import spec
from spec.schema import Capability, CppProjection, Spec

EXPECTED_CAPABILITIES = {
    "concepts": ("__cpp_concepts", 201907, None, None),
    "consteval": ("__cpp_consteval", 201811, None, None),
    "constexpr_dynamic_alloc": ("__cpp_constexpr_dynamic_alloc", 201907, None, None),
    "copyable_function": ("__cpp_lib_copyable_function", 202306, None, "version"),
    "deleted_function_reason": ("__cpp_deleted_function", 202403, None, None),
    "if_consteval": ("__cpp_if_consteval", 202106, None, None),
    "move_only_function": ("__cpp_lib_move_only_function", 202110, None, "version"),
    "pack_indexing": ("__cpp_pack_indexing", 202311, None, None),
    "static_assert_message": ("__cpp_static_assert", 202306, None, None),
    "type_pack_element": (None, None, "__type_pack_element", None),
}


def test_load_spec_returns_a_spec():
    loaded = spec.load_spec()
    assert isinstance(loaded, Spec)
    assert isinstance(loaded.cpp, CppProjection)


def test_capabilities_are_sorted_by_id():
    capabilities = spec.load_spec().cpp.capabilities
    assert isinstance(capabilities, tuple)
    assert len(capabilities) == 10
    assert [c.id for c in capabilities] == sorted(c.id for c in capabilities)


def test_capability_content():
    capabilities = spec.load_spec().cpp.capabilities
    assert all(isinstance(c, Capability) for c in capabilities)
    actual = {
        c.id: (c.feature_macro, c.minimum, c.builtin, c.header) for c in capabilities
    }
    assert actual == EXPECTED_CAPABILITIES
    assert all(c.description.endswith(".") for c in capabilities)


def test_every_section_is_a_tuple():
    loaded = spec.load_spec()
    for field in dataclasses.fields(Spec):
        if field.name != "cpp":
            assert isinstance(getattr(loaded, field.name), tuple), field.name
    for field in dataclasses.fields(spec.CppProjection):
        assert isinstance(getattr(loaded.cpp, field.name), tuple), field.name


def test_every_section_has_a_content_module():
    import importlib

    for field in dataclasses.fields(Spec):
        if field.name != "cpp":
            module = importlib.import_module(f"spec.content.{field.name}")
            assert hasattr(module, field.name)
    for field in dataclasses.fields(CppProjection):
        module = importlib.import_module(f"spec.content.cpp_{field.name}")
        assert hasattr(module, f"cpp_{field.name}")


def test_package_exports_schema_types():
    for name in ("Spec", "Capability", "Scope", "Rule", "Diagnostic", "Contract"):
        assert getattr(spec, name) is getattr(spec.schema, name)
    assert callable(spec.validate)
