#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

import sys
from dataclasses import replace
from pathlib import Path

from schema import (
    ResolvedType,
)


ROOT_DIR = Path(__file__).resolve().parents[3]
if str(ROOT_DIR) not in sys.path:
    sys.path.insert(0, str(ROOT_DIR))

from tools.model.legacy.request import (  # noqa: E402
    concrete_requests,
    interface_requests,
    qualified_requests,
)


_CONCRETE = dict(concrete_requests())
_QUALIFIED = dict(qualified_requests())
_INTERFACE = dict(interface_requests())


def _derived(name: str, exposed: set[str] | None = None) -> ResolvedType:
    """A concrete request whose resolved facts come from the request algebra."""
    facts = _CONCRETE[name]
    return ResolvedType(
        name=name,
        supported_exposed_types=frozenset(exposed or {"concrete"}),
        provides=facts.provides,
        requires=facts.requires,
        dependency_forms=facts.dependency_forms,
        supported_modes=facts.supported_modes,
    )


def _qualified(name: str, exposed: set[str]) -> ResolvedType:
    """A request whose facts come from an annotated/keyed qualifier."""
    facts = _QUALIFIED[name]
    return ResolvedType(
        name=name,
        supported_exposed_types=frozenset(exposed),
        provides=facts.provides,
        requires=facts.requires,
        dependency_forms=facts.dependency_forms,
        supported_modes=facts.supported_modes,
    )


def _interface(name: str, exposed: set[str]) -> ResolvedType:
    """A request whose facts come from an interface exposure policy."""
    facts = _INTERFACE[name]
    return ResolvedType(
        name=name,
        supported_exposed_types=frozenset(exposed),
        provides=facts.provides,
        requires=facts.requires,
        dependency_forms=facts.dependency_forms,
        supported_modes=facts.supported_modes,
    )


def _row(
    name: str,
    exposed: set[str],
    provides: set[str],
    *,
    requires: set[str] | None = None,
    dependency_forms: set[str] | None = None,
    modes: set[str] | None = None,
) -> ResolvedType:
    """One selected resolved row whose facts are feature policy, not algebra."""
    optional: dict[str, object] = {}
    if modes is not None:
        optional["supported_modes"] = frozenset(modes)
    return ResolvedType(
        name=name,
        supported_exposed_types=frozenset(exposed),
        provides=frozenset(provides),
        requires=frozenset(requires or ()),
        dependency_forms=frozenset(dependency_forms or ()),
        **optional,
    )


RESOLVED_TYPES = (
    _derived("value_ref_ptr", {"concrete", "interface_type"}),
    _derived("value"),
    _derived("value_rvalue"),
    _row("dependent_type_ref", {"mixed_external_dependency"}, {"resolved_mixed_external_dependency"}),
    _row("explicit_dependencies_value", {"explicit_dependencies"}, {"resolved_explicit_dependencies"}),
    _row("mixed_singular_conflict_ref", {"mixed_singular_conflict"}, {"resolved_mixed_singular_conflict"}, requires={"direct_value_resolution", "stable_concrete_storage"}),
    _derived("const_value_ref"),
    _derived("const_value_ptr"),
    _interface("interface_value", {"copyable_interface_type"}),
    _interface("interface_ref_ptr_shared_ptr", {"interface_type"}),
    _interface("interface_ref", {"interface_type", "multiple_interfaces"}),
    _interface("interface_ptr", {"interface_type", "multiple_interfaces"}),
    _interface("second_interface_ref", {"multiple_interfaces"}),
    _interface("multiple_interface_shared_ptr_ref", {"multiple_interfaces"}),
    _interface("unique_interface_unique_ptr", {"unique_interface_type"}),
    _derived("value_unique_ptr"),
    _interface("interface_unique_ptr", {"unique_interface_type"}),
    _derived("value_shared_ptr"),
    _derived("value_shared_ptr_ref"),
    _derived("value_optional"),
    _derived("value_optional_ref"),
    _interface("interface_shared_ptr", {"interface_type"}),
    _row("custom_shared_concrete", {"custom_concrete"}, {"resolved_custom_wrapper"}, requires={"stable_concrete_storage", "stored_custom_shared"}),
    _row("custom_shared_interface", {"custom_interface"}, {"resolved_custom_wrapper"}, requires={"shared_storage", "stored_custom_shared"}),
    _row("custom_unique_interface", {"custom_unique_interface"}, {"resolved_custom_wrapper"}, requires={"stored_custom_unique", "unique_storage"}),
    _row("custom_optional_ref", {"custom_concrete"}, {"resolved_custom_wrapper"}, requires={"stable_concrete_storage", "stored_custom_optional"}),
    _row("custom_optional_value", {"custom_concrete"}, {"resolved_custom_wrapper"}, requires={"stored_custom_optional", "unique_storage"}),
    _row("element_vector", {"element_collection"}, {"constructable_collection", "resolved_collection"}, modes={"mixed", "static"}),
    _row("element_map_custom_insert", {"element_collection"}, {"constructable_collection"}, modes={"mixed", "static"}),
    _row("element_keyed_shared_ptr_ref", {"element_keyed"}, {"resolved_keyed"}),
    _qualified("keyed_value_dependency", {"keyed_concrete"}),
    _qualified("keyed_value", {"keyed_concrete"}),
    _qualified("keyed_value_ref", {"keyed_concrete"}),
    _qualified("keyed_interface_ref", {"keyed_interface"}),
    _row("element_keyed_vector", {"element_keyed_collection"}, {"resolved_keyed_collection"}, modes={"mixed", "static"}),
    _row("keyed_collection_dependency", {"element_keyed_collection"}, {"constructable_dependency"}, dependency_forms={"selected_value"}, modes={"mixed", "static"}),
    _row("element_indexed_shared_ptr", {"element_indexed"}, {"resolved_indexed"}, modes={"runtime"}),
    _row("raw_array_ptr", {"array_concrete"}, {"resolved_array"}, requires={"array_1d"}),
    _row("raw_array_ref", {"array_concrete"}, {"resolved_array"}, requires={"array_1d"}, dependency_forms={"array_lvalue_reference"}),
    _row("raw_array_ptr_to_array", {"array_concrete"}, {"resolved_array"}, requires={"array_1d"}),
    _row("raw_nd_array_ref", {"array_concrete"}, {"resolved_array"}, requires={"array_2d"}),
    _row("unique_array", {"array_concrete"}, {"resolved_array"}, requires={"array_smart_unique", "unique_storage"}),
    _row("shared_array", {"array_concrete"}, {"resolved_array"}, requires={"array_smart_shared", "shared_storage"}),
    _row("shared_unique_value_ref", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_shared_unique"}),
    _row("shared_unique_array_value_ref", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_shared_unique_array"}),
    _row("unique_shared_value", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_unique_shared"}),
    _row("variant_unique_value", {"variant_unique_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_variant_unique"}),
    _row("variant_shared_value", {"variant_shared_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_variant_shared"}),
    _row("variant_shared_unique_value", {"variant_shared_unique_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_variant_shared_unique"}),
    _row("unique_variant_value", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_unique_variant"}),
    _row("shared_variant_value_ref", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_shared_variant"}),
    _row("array_variant_value_ref", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_array_variant"}),
    _row("leaf_access", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_leaf_access"}),
    _row("leaf_copy", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"stored_leaf_copy"}),
    _row("leaf_access_move_only", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"runtime_refusal_container", "stored_leaf_access_move_only"}),
    _row("leaf_refusal", {"nested_wrapper"}, {"resolved_nested_wrapper"}, requires={"runtime_refusal_container", "stored_leaf_refusal"}),
    _row("variant_value", {"variant_concrete"}, {"resolved_variant"}, dependency_forms={"variant"}),
    _row("variant_alternative_value", {"variant_concrete"}, {"resolved_variant", "resolved_variant_alternative"}),
    _row("variant_alternative_ref", {"variant_concrete"}, {"resolved_variant", "resolved_variant_alternative"}, requires={"stable_concrete_storage"}),
    _row("variant_alternative_ptr", {"variant_concrete"}, {"resolved_variant", "resolved_variant_alternative"}, requires={"stable_concrete_storage"}),
    _qualified("annotated_value_ref", {"annotated_concrete"}),
    _qualified("annotated_interface_ref", {"annotated_interface"}),
    _qualified("annotated_interface_ptr", {"annotated_interface"}),
    _qualified("annotated_interface_shared_ptr", {"annotated_interface"}),
    _qualified("annotated_interface_shared_ptr_ref", {"annotated_interface"}),
    _row("local_value_ref", {"local_value"}, {"resolved_concrete"}),
    _row("local_override_value_ref", {"local_override_value"}, {"resolved_concrete"}),
    _row("local_collection_value_ref", {"local_collection_runtime_host_value", "local_collection_value"}, {"resolved_concrete"}),
    _row("factory_function_value_ref", {"factory_function_value"}, {"resolved_concrete"}),
    _row("factory_function_dependency_value_ref", {"factory_function_dependency_value"}, {"resolved_concrete"}),
    _row("factory_constructor_value_ref", {"factory_constructor_value"}, {"resolved_concrete"}),
    _row("factory_detected_constructor_value_ref", {"factory_detected_constructor_value"}, {"resolved_concrete"}),
    _row("factory_typedef_constructor_value_ref", {"factory_typedef_constructor_value"}, {"resolved_concrete"}),
    _row("factory_callable_value_ref", {"factory_callable_value"}, {"resolved_concrete"}, modes={"mixed", "runtime"}),
)


# Python selects and shards valid combinations; C++ owns the scenario behavior.
# Keeping this map separate from the axis definitions makes the generated rows
# small while preserving one independently compiled source per feature case.
_RESOLUTION_POLICIES = {
    ("value_ref_ptr", "resolve_concrete"): "resolution::ref_ptr<value_type>",
    ("value", "resolve_concrete"): "resolution::value<value_type>",
    (
        "keyed_value_dependency",
        "construct",
    ): "resolution::construct_member_value<keyed_value_dependency_type, 3>",
    (
        "keyed_collection_dependency",
        "construct",
    ): "resolution::construct_count_sum<keyed_collection_dependency_type>",
    (
        "dependent_type_ref",
        "mixed_external_dependency",
    ): "resolution::member_value_ref<dependent_type, 3>",
    ("const_value_ref", "resolve_concrete"): "resolution::const_ref<value_type>",
    ("const_value_ptr", "resolve_concrete"): "resolution::const_ptr<value_type>",
    (
        "interface_value",
        "resolve_interface",
    ): "resolution::constructed<resolution::requests::direct<copyable_interface_type>>",
    (
        "interface_ref_ptr_shared_ptr",
        "resolve_interface",
    ): "resolution::interface_ref_ptr_shared_ptr<interface_type>",
    (
        "interface_ref",
        "resolve_interface",
    ): "resolution::constructed<resolution::interface_ref<interface_type>>",
    (
        "interface_ptr",
        "resolve_interface",
    ): "resolution::pointee_constructed<resolution::interface_ptr<interface_type>>",
    (
        "second_interface_ref",
        "resolve_interface",
    ): (
        "resolution::multiple_interface_identity<implementation_type, "
        "interface_type, second_interface_type, 4>"
    ),
    (
        "multiple_interface_shared_ptr_ref",
        "resolve_interface",
    ): (
        "resolution::multiple_interface_shared_identity<implementation_type, "
        "interface_type, second_interface_type, 4>"
    ),
    (
        "unique_interface_unique_ptr",
        "resolve_unique_wrapper",
    ): "resolution::unique_ptr<unique_interface_type>",
    (
        "value_unique_ptr",
        "resolve_wrapper",
    ): "resolution::unique_ptr<value_type>",
    (
        "interface_unique_ptr",
        "resolve_wrapper",
    ): "resolution::unique_ptr<unique_interface_type>",
    (
        "value_shared_ptr",
        "resolve_wrapper",
    ): "resolution::shared_ptr<value_type>",
    (
        "value_shared_ptr_ref",
        "resolve_wrapper",
    ): "resolution::shared_ptr_ref<value_type>",
    ("value_optional", "resolve_wrapper"): "resolution::optional<value_type>",
    (
        "value_optional_ref",
        "resolve_wrapper",
    ): "resolution::optional_ref<value_type>",
    (
        "interface_shared_ptr",
        "resolve_wrapper",
    ): "resolution::pointee_constructed<resolution::interface_shared_ptr<interface_type>>",
    (
        "element_vector",
        "resolve_collection",
    ): "resolution::collection<std::vector<std::shared_ptr<element_interface>>>",
    (
        "element_map_custom_insert",
        "construct_collection",
    ): "resolution::construct_collection<std::map<int, std::shared_ptr<element_interface>>>",
    (
        "custom_shared_concrete",
        "custom_wrappers",
    ): "resolution::custom_shared<value_type, dingo::test_shared>",
    (
        "custom_shared_interface",
        "custom_wrappers",
    ): "resolution::custom_shared<interface_type, dingo::test_shared>",
    (
        "custom_unique_interface",
        "custom_wrappers",
    ): "resolution::custom_unique<unique_interface_type, dingo::test_unique, dingo::test_shared>",
    (
        "custom_optional_ref",
        "custom_wrappers",
    ): "resolution::custom_optional_ref<value_type, dingo::test_optional>",
    (
        "custom_optional_value",
        "custom_wrappers",
    ): "resolution::custom_optional<value_type, dingo::test_optional>",
    (
        "element_keyed_shared_ptr_ref",
        "resolve_keyed",
    ): "resolution::keyed_shared_ptr_refs<element_interface, dingo::key_type<key_a>, dingo::key_type<key_b>>",
    (
        "keyed_value",
        "resolve_keyed",
    ): "resolution::keyed_value<value_type, dingo::key_type<key_a>>",
    (
        "keyed_value_ref",
        "resolve_keyed",
    ): "resolution::keyed_ref<value_type, dingo::key_type<key_a>>",
    (
        "keyed_interface_ref",
        "resolve_keyed",
    ): "resolution::keyed_ref<interface_type, dingo::key_type<key_a>>",
    (
        "element_keyed_vector",
        "resolve_keyed_collection",
    ): "resolution::keyed_collection<std::vector<std::shared_ptr<element_interface>>, dingo::key_type<key_a>>",
    (
        "element_indexed_shared_ptr",
        "resolve_indexed",
    ): "resolution::indexed_shared_ptrs<element_interface>",
    ("raw_array_ptr", "array"): "resolution::raw_array_ptr<value_type>",
    ("raw_array_ref", "array"): "resolution::raw_array_ref<value_type, 2>",
    (
        "raw_array_ptr_to_array",
        "array",
    ): "resolution::raw_array_ptr_to_array<value_type, 2>",
    (
        "raw_nd_array_ref",
        "array",
    ): "resolution::raw_nd_array_ref<value_type, 2, 3>",
    ("unique_array", "array"): "resolution::unique_array<value_type>",
    ("shared_array", "array"): "resolution::shared_array<value_type>",
    (
        "shared_unique_value_ref",
        "nested_wrappers",
    ): "resolution::shared_unique_ref<shared_unique_value_type, std::unique_ptr<value_type>, value_type>",
    (
        "shared_unique_array_value_ref",
        "nested_wrappers",
    ): "resolution::shared_unique_array_ref<shared_unique_array_value_type, std::unique_ptr<value_type[]>, value_type>",
    (
        "unique_shared_value",
        "nested_wrappers",
    ): "resolution::nested_unique_value<unique_shared_value_type>",
    (
        "variant_unique_value",
        "nested_wrappers",
    ): "resolution::variant_unique<variant_unique_value_type, std::unique_ptr<value_type>>",
    (
        "variant_shared_value",
        "nested_wrappers",
    ): "resolution::variant_shared_ref<variant_shared_value_type, std::shared_ptr<value_type>>",
    (
        "variant_shared_unique_value",
        "nested_wrappers",
    ): "resolution::variant_shared_unique_ref<variant_shared_unique_value_type, std::shared_ptr<std::unique_ptr<value_type>>, std::unique_ptr<value_type>, value_type>",
    (
        "unique_variant_value",
        "nested_wrappers",
    ): "resolution::unique_variant<unique_variant_value_type, nested_variant_a>",
    (
        "shared_variant_value_ref",
        "nested_wrappers",
    ): "resolution::shared_variant_ref<shared_variant_value_type, nested_variant_a>",
    (
        "array_variant_value_ref",
        "nested_wrappers",
    ): "resolution::array_variant_ref<array_variant_value_type, std::array<value_type, 2>>",
    (
        "leaf_access",
        "nested_wrappers",
    ): "resolution::leaf_access<value_type>",
    (
        "leaf_copy",
        "nested_wrappers",
    ): "resolution::leaf_copy<value_type>",
    (
        "leaf_access_move_only",
        "nested_wrappers",
    ): "resolution::leaf_access_move_only<move_only_leaf_type>",
    (
        "leaf_refusal",
        "nested_wrappers",
    ): "resolution::leaf_refusal<move_only_leaf_type>",
    (
        "variant_value",
        "variant",
    ): "resolution::variant_value<std::variant<variant_a, variant_b>, variant_a>",
    (
        "variant_alternative_value",
        "variant",
    ): "resolution::member_value<variant_a, 3>",
    (
        "variant_alternative_ref",
        "variant",
    ): "resolution::member_value_ref<variant_a, 3>",
    (
        "variant_alternative_ptr",
        "variant",
    ): "resolution::member_value_ptr<variant_a, 3>",
    (
        "annotated_value_ref",
        "annotated",
    ): "resolution::annotated_value<dingo::annotated<value_type&, tag_a>>",
    (
        "annotated_interface_ref",
        "annotated",
    ): "resolution::annotated_value<dingo::annotated<interface_type&, tag_a>>",
    (
        "annotated_interface_ptr",
        "annotated",
    ): "resolution::annotated_pointee<dingo::annotated<interface_type*, tag_a>>",
    (
        "annotated_interface_shared_ptr",
        "annotated",
    ): "resolution::annotated_pointee<dingo::annotated<std::shared_ptr<interface_type>, tag_a>>",
    (
        "annotated_interface_shared_ptr_ref",
        "annotated",
    ): "resolution::annotated_pointee<dingo::annotated<std::shared_ptr<interface_type>&, tag_a>>",
    (
        "local_value_ref",
        "local_bindings",
    ): "resolution::local_value_ref<local_value_type, 12>",
    (
        "local_override_value_ref",
        "local_bindings",
    ): "resolution::local_value_ref<local_override_value_type, 4>",
    (
        "local_collection_value_ref",
        "local_bindings",
    ): "resolution::local_collection_ref<local_collection_value_type>",
}

RESOLVED_TYPES = tuple(
    replace(
        resolved_type,
        policies=tuple(
            (feature, policy)
            for (name, feature), policy in _RESOLUTION_POLICIES.items()
            if name == resolved_type.name
        ),
    )
    for resolved_type in RESOLVED_TYPES
)
