#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import replace

from schema import (
    ResolvedType,
)


RESOLVED_TYPES = (
    ResolvedType(
        name="scenario",
        supported_exposed_types=frozenset({"scenario"}),
        provides=frozenset({"scenario"}),
    ),
    ResolvedType(
        name="value_ref_ptr",
        supported_exposed_types=frozenset({"concrete", "interface_type"}),
        provides=frozenset(
            {
                "resolved_concrete",
                "constructable_dependency",
                "invokable_dependency",
                "unkeyed_invokable_dependency",
            }
        ),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
    ),
    ResolvedType(
        name="value",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_concrete_value"}),
        requires=frozenset({"direct_value_resolution"}),
    ),
    ResolvedType(
        name="value_rvalue",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete"}),
        requires=frozenset({"unique_storage", "stored_value"}),
    ),
    ResolvedType(
        name="cycle_ref",
        supported_exposed_types=frozenset({"cycle_concrete"}),
        provides=frozenset({"resolved_concrete"}),
        requires=frozenset({"cycle_graph"}),
    ),
    ResolvedType(
        name="dependent_type_ref",
        supported_exposed_types=frozenset({"mixed_external_dependency"}),
        provides=frozenset({"resolved_mixed_external_dependency"}),
    ),
    ResolvedType(
        name="explicit_dependencies_value",
        supported_exposed_types=frozenset({"explicit_dependencies"}),
        provides=frozenset({"resolved_explicit_dependencies"}),
    ),
    ResolvedType(
        name="mixed_singular_conflict_ref",
        supported_exposed_types=frozenset({"mixed_singular_conflict"}),
        provides=frozenset({"resolved_mixed_singular_conflict"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
    ),
    ResolvedType(
        name="const_value_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_const_concrete"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
    ),
    ResolvedType(
        name="const_value_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_const_concrete"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
    ),
    ResolvedType(
        name="interface_value",
        supported_exposed_types=frozenset({"copyable_interface_type"}),
        provides=frozenset({"resolved_interface", "resolved_interface_value"}),
    ),
    ResolvedType(
        name="interface_ref_ptr_shared_ptr",
        supported_exposed_types=frozenset({"interface_type"}),
        provides=frozenset({"resolved_interface", "resolved_shared_interface"}),
        requires=frozenset({"shared_storage"}),
    ),
    ResolvedType(
        name="interface_ref",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
    ),
    ResolvedType(
        name="interface_ptr",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
    ),
    ResolvedType(
        name="second_interface_ref",
        supported_exposed_types=frozenset({"multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
    ),
    ResolvedType(
        name="multiple_interface_shared_ptr_ref",
        supported_exposed_types=frozenset({"multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset(
            {"runtime_regression_container", "shared_storage"}
        ),
        supported_modes=frozenset({"runtime"}),
    ),
    ResolvedType(
        name="unique_interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface_type"}),
        provides=frozenset({"resolved_unique_wrapper"}),
        requires=frozenset({"unique_storage"}),
    ),
    ResolvedType(
        name="value_unique_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_unique_ptr"}),
    ),
    ResolvedType(
        name="interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface_type"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_unique_ptr"}),
    ),
    ResolvedType(
        name="value_shared_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
    ),
    ResolvedType(
        name="value_shared_ptr_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
    ),
    ResolvedType(
        name="value_optional",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_optional"}),
    ),
    ResolvedType(
        name="value_optional_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_optional"}),
    ),
    ResolvedType(
        name="interface_shared_ptr",
        supported_exposed_types=frozenset({"interface_type"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
    ),
    ResolvedType(
        name="custom_shared_concrete",
        supported_exposed_types=frozenset({"custom_concrete"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"stable_concrete_storage", "stored_custom_shared"}),
    ),
    ResolvedType(
        name="custom_shared_interface",
        supported_exposed_types=frozenset({"custom_interface"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"shared_storage", "stored_custom_shared"}),
    ),
    ResolvedType(
        name="custom_unique_interface",
        supported_exposed_types=frozenset({"custom_unique_interface"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"unique_storage", "stored_custom_unique"}),
    ),
    ResolvedType(
        name="custom_optional_ref",
        supported_exposed_types=frozenset({"custom_concrete"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"stable_concrete_storage", "stored_custom_optional"}),
    ),
    ResolvedType(
        name="custom_optional_value",
        supported_exposed_types=frozenset({"custom_concrete"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"unique_storage", "stored_custom_optional"}),
    ),
    ResolvedType(
        name="element_vector",
        supported_exposed_types=frozenset({"element_collection"}),
        provides=frozenset({"resolved_collection", "constructable_collection"}),
        supported_modes=frozenset({"static", "mixed"}),
    ),
    ResolvedType(
        name="element_map_custom_insert",
        supported_exposed_types=frozenset({"element_collection"}),
        provides=frozenset({"constructable_collection"}),
        supported_modes=frozenset({"static", "mixed"}),
    ),
    ResolvedType(
        name="element_keyed_shared_ptr_ref",
        supported_exposed_types=frozenset({"element_keyed"}),
        provides=frozenset({"resolved_keyed"}),
    ),
    ResolvedType(
        name="keyed_value_dependency",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"constructable_dependency", "invokable_dependency"}),
        requires=frozenset({"direct_value_resolution", "stable_concrete_storage"}),
    ),
    ResolvedType(
        name="keyed_value",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"direct_value_resolution"}),
    ),
    ResolvedType(
        name="keyed_value_ref",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"direct_value_resolution", "stable_concrete_storage"}),
    ),
    ResolvedType(
        name="keyed_interface_ref",
        supported_exposed_types=frozenset({"keyed_interface"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"shared_storage"}),
    ),
    ResolvedType(
        name="element_keyed_vector",
        supported_exposed_types=frozenset({"element_keyed_collection"}),
        provides=frozenset({"resolved_keyed_collection"}),
        supported_modes=frozenset({"static", "mixed"}),
    ),
    ResolvedType(
        name="keyed_collection_dependency",
        supported_exposed_types=frozenset({"element_keyed_collection"}),
        provides=frozenset({"constructable_dependency", "invokable_dependency"}),
        supported_modes=frozenset({"static", "mixed"}),
    ),
    ResolvedType(
        name="element_indexed_shared_ptr",
        supported_exposed_types=frozenset({"element_indexed"}),
        provides=frozenset({"resolved_indexed"}),
        supported_modes=frozenset({"runtime"}),
    ),
    ResolvedType(
        name="raw_array_ptr",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
    ),
    ResolvedType(
        name="raw_array_ref",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
    ),
    ResolvedType(
        name="raw_array_ptr_to_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
    ),
    ResolvedType(
        name="raw_nd_array_ref",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_2d"}),
    ),
    ResolvedType(
        name="unique_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"unique_storage", "array_smart_unique"}),
    ),
    ResolvedType(
        name="shared_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"shared_storage", "array_smart_shared"}),
    ),
    ResolvedType(
        name="shared_unique_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_shared_unique"}),
    ),
    ResolvedType(
        name="shared_unique_array_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_shared_unique_array"}),
    ),
    ResolvedType(
        name="unique_shared_value",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_unique_shared"}),
    ),
    ResolvedType(
        name="variant_unique_value",
        supported_exposed_types=frozenset({"variant_unique_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_variant_unique"}),
    ),
    ResolvedType(
        name="variant_shared_value",
        supported_exposed_types=frozenset({"variant_shared_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_variant_shared"}),
    ),
    ResolvedType(
        name="variant_shared_unique_value",
        supported_exposed_types=frozenset({"variant_shared_unique_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_variant_shared_unique"}),
    ),
    ResolvedType(
        name="unique_variant_value",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_unique_variant"}),
    ),
    ResolvedType(
        name="shared_variant_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_shared_variant"}),
    ),
    ResolvedType(
        name="array_variant_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_array_variant"}),
    ),
    ResolvedType(
        name="variant_value",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant"}),
    ),
    ResolvedType(
        name="variant_alternative_value",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
    ),
    ResolvedType(
        name="variant_alternative_ref",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        requires=frozenset({"stable_concrete_storage"}),
    ),
    ResolvedType(
        name="variant_alternative_ptr",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        requires=frozenset({"stable_concrete_storage"}),
    ),
    ResolvedType(
        name="annotated_value_ref",
        supported_exposed_types=frozenset({"annotated_concrete"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"stable_concrete_storage"}),
    ),
    ResolvedType(
        name="annotated_interface_ref",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage"}),
    ),
    ResolvedType(
        name="annotated_interface_ptr",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage"}),
    ),
    ResolvedType(
        name="annotated_interface_shared_ptr",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
    ),
    ResolvedType(
        name="annotated_interface_shared_ptr_ref",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
    ),
    ResolvedType(
        name="local_value_ref",
        supported_exposed_types=frozenset({"local_value"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="local_override_value_ref",
        supported_exposed_types=frozenset({"local_override_value"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="local_collection_value_ref",
        supported_exposed_types=frozenset(
            {"local_collection_value", "local_collection_runtime_host_value"}
        ),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="factory_function_value_ref",
        supported_exposed_types=frozenset({"factory_function_value"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="factory_function_dependency_value_ref",
        supported_exposed_types=frozenset({"factory_function_dependency_value"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="factory_constructor_value_ref",
        supported_exposed_types=frozenset({"factory_constructor_value"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="factory_detected_constructor_value_ref",
        supported_exposed_types=frozenset({"factory_detected_constructor_value"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="factory_typedef_constructor_value_ref",
        supported_exposed_types=frozenset({"factory_typedef_constructor_value"}),
        provides=frozenset({"resolved_concrete"}),
    ),
    ResolvedType(
        name="factory_callable_value_ref",
        supported_exposed_types=frozenset({"factory_callable_value"}),
        provides=frozenset({"resolved_concrete"}),
        supported_modes=frozenset({"runtime", "mixed"}),
    ),
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
        "keyed_value_dependency",
        "invoke",
    ): "resolution::keyed_invoke<value_type, dingo::key_type<key_a>, 3>",
    (
        "keyed_collection_dependency",
        "construct",
    ): "resolution::construct_count_sum<keyed_collection_dependency_type>",
    (
        "keyed_collection_dependency",
        "invoke",
    ): "resolution::keyed_collection_invoke<std::vector<std::shared_ptr<element_interface>>, dingo::key_type<key_a>, 2>",
    ("cycle_ref", "resolve_concrete"): "resolution::cycle_ref<cycle_a_type>",
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
