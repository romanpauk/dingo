#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from dataclasses import replace

from schema import FeatureCaseSpec, MixedRegistrationPlacement, RegistrationSpec


VALUE_DEPENDENCY = frozenset(
    {
        "direct_value_resolution",
        "stable_concrete_storage",
        "unkeyed_invokable_dependency",
    }
)
PARENT_MATRIX = frozenset(
    {
        "cross_parent_container_matrix",
        "stable_concrete_storage",
        "stored_value",
    }
)


FEATURE_CASES = (
    FeatureCaseSpec(
        name="native_parent_container_shape",
        feature="nested_container",
        headers=("matrix/scenarios/parent/native.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_resolution",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/resolution.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_ownership",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/ownership.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_origin",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/origin.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_cache",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/cache.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_transactions",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/transactions.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_recursion",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/recursion.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_semantics",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/semantics.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_registration_transactions",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/semantics.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_allocators",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/allocators.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_collections",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/collections.h",),
    ),
    FeatureCaseSpec(
        name="cross_parent_intermediate_ambiguity",
        feature="nested_container",
        requires=PARENT_MATRIX,
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        headers=("matrix/scenarios/parent/search.h",),
    ),
    FeatureCaseSpec(
        name="container_child_container_parent",
        feature="static_parent_container",
        requires=frozenset({"static_parent_container_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
    ),
    FeatureCaseSpec(
        name="container_child_static_parent",
        feature="static_parent_container",
        requires=frozenset({"static_parent_container_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
    ),
    FeatureCaseSpec(
        name="static_child_static_parent",
        feature="static_parent_container",
        requires=frozenset({"static_parent_static_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
    ),
    FeatureCaseSpec(
        name="mixed_runtime_static_pairs",
        feature="static_parent_container",
        requires=frozenset({"static_parent_container_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
    ),
    FeatureCaseSpec(
        name="inferred_lambda",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="explicit_signature_overload",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="std_function",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        system_headers=("functional",),
    ),
    FeatureCaseSpec(
        name="free_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="noexcept_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="static_member_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="member_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="mutable_lambda",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="generic_lambda_explicit_signature",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="lvalue_ref_qualified_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="rvalue_ref_qualified_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="noexcept_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="move_only_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="multi_argument_callable",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
    ),
    FeatureCaseSpec(
        name="move_only_function",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        system_headers=("functional",),
    ),
    FeatureCaseSpec(
        name="keyed_value_dependency",
        feature="invoke",
        requires=frozenset({"shared_storage", "stored_value"}),
        supported_exposed_types=frozenset({"keyed_concrete"}),
        supported_resolved_types=frozenset({"keyed_value_dependency"}),
    ),
    FeatureCaseSpec(
        name="keyed_collection_dependency",
        feature="invoke",
        supported_exposed_types=frozenset({"element_keyed_collection"}),
        supported_resolved_types=frozenset({"keyed_collection_dependency"}),
    ),
    FeatureCaseSpec(
        name="factory_function_no_args",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_function_value"}),
        supported_resolved_types=frozenset({"factory_function_value_ref"}),
        registrations=(
            RegistrationSpec(
                factory="dingo::factory<dingo::function<&factory_function_value_type::create>>"
            ),
        ),
    ),
    FeatureCaseSpec(
        name="factory_function_with_dependency",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_function_dependency_value"}),
        supported_resolved_types=frozenset({"factory_function_dependency_value_ref"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                factory="dingo::factory<dingo::function<&factory_function_dependency_value_type::create>>"
            ),
        ),
    ),
    FeatureCaseSpec(
        name="explicit_constructor_factory",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_constructor_value"}),
        supported_resolved_types=frozenset({"factory_constructor_value_ref"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                factory="dingo::factory<dingo::constructor<factory_constructor_value_type(value_type&)>>"
            ),
        ),
    ),
    FeatureCaseSpec(
        name="detected_constructor",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_detected_constructor_value"}),
        supported_resolved_types=frozenset({"factory_detected_constructor_value_ref"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(),
        ),
    ),
    FeatureCaseSpec(
        name="typedef_constructor",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_typedef_constructor_value"}),
        supported_resolved_types=frozenset({"factory_typedef_constructor_value_ref"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(),
        ),
    ),
    FeatureCaseSpec(
        name="callable_registration_factory",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_callable_value"}),
        supported_resolved_types=frozenset({"factory_callable_value_ref"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(
                runtime_argument=(
                    "dingo::callable([](value_type& dependency) { "
                    "return factory_callable_value_type(dependency.marker() + 12); })"
                ),
                mixed_placement=MixedRegistrationPlacement.RUNTIME,
                include_in_static=False,
            ),
        ),
    ),
    FeatureCaseSpec(
        name="explicit_callable_construct",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_callable_value"}),
        supported_resolved_types=frozenset({"factory_callable_value_ref"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
        ),
    ),
    FeatureCaseSpec(name="unkeyed", feature="lookup_route_regressions"),
    FeatureCaseSpec(name="typed", feature="lookup_route_regressions"),
    FeatureCaseSpec(name="key_value", feature="lookup_route_regressions"),
)


_FEATURE_CASE_POLICIES = {
    ("lookup_route_regressions", "unkeyed"): (
        "resolution::scenario<unkeyed_lookup_route_scenario>"
    ),
    ("lookup_route_regressions", "typed"): (
        "resolution::scenario<typed_lookup_route_scenario>"
    ),
    ("lookup_route_regressions", "key_value"): (
        "resolution::scenario<key_value_lookup_route_scenario>"
    ),
    ("nested_container", "native_parent_container_shape"): (
        "resolution::scenario<native_parent_container_scenario>"
    ),
    ("nested_container", "cross_parent_resolution"): (
        "resolution::no_container_function<&exercise_parent_resolution_pairs<>>"
    ),
    ("nested_container", "cross_parent_ownership"): (
        "resolution::no_container_function<&exercise_parent_ownership_pairs<>>"
    ),
    ("nested_container", "cross_parent_origin"): (
        "resolution::no_container_function<&exercise_parent_origin_chains<>>"
    ),
    ("nested_container", "cross_parent_cache"): (
        "resolution::no_container_function<&exercise_parent_cache_pairs<>>"
    ),
    ("nested_container", "cross_parent_transactions"): (
        "resolution::no_container_function<&exercise_parent_transaction_pairs<>>"
    ),
    ("nested_container", "cross_parent_recursion"): (
        "resolution::no_container_function<&exercise_parent_recursion_pairs<>>"
    ),
    ("nested_container", "cross_parent_semantics"): (
        "resolution::no_container_function<&exercise_parent_semantics>"
    ),
    ("nested_container", "cross_parent_registration_transactions"): (
        "resolution::no_container_function<"
        "&exercise_parent_registration_transactions>"
    ),
    ("nested_container", "cross_parent_allocators"): (
        "resolution::no_container_function<&exercise_parent_allocator_pairs<>>"
    ),
    ("nested_container", "cross_parent_collections"): (
        "resolution::no_container_function<&exercise_parent_collection_pairs<>>"
    ),
    ("nested_container", "cross_parent_intermediate_ambiguity"): (
        "resolution::no_container_function<&exercise_intermediate_ambiguity_chains<>>"
    ),
    ("static_parent_container", "container_child_container_parent"): (
        "resolution::no_container_function<&exercise_static_parent_container_pair<"
        "container_parent_shape, container_child_shape>>"
    ),
    ("static_parent_container", "container_child_static_parent"): (
        "resolution::no_container_function<&exercise_static_parent_container_pair<"
        "static_parent_shape, container_child_shape>>"
    ),
    ("static_parent_container", "static_child_static_parent"): (
        "resolution::no_container_function<&exercise_static_parent_container_pair<"
        "static_parent_shape, static_child_shape>>"
    ),
    ("static_parent_container", "mixed_runtime_static_pairs"): (
        "resolution::no_container_function<&exercise_mixed_runtime_static_parent_pairs>"
    ),
    ("invoke", "inferred_lambda"): "resolution::scenario<invoke_inferred_lambda>",
    ("invoke", "explicit_signature_overload"): (
        "resolution::scenario<invoke_explicit_signature_overload>"
    ),
    ("invoke", "std_function"): "resolution::scenario<invoke_std_function>",
    ("invoke", "free_function_pointer"): (
        "resolution::scenario<invoke_free_function_pointer>"
    ),
    ("invoke", "noexcept_function_pointer"): (
        "resolution::scenario<invoke_noexcept_function_pointer>"
    ),
    ("invoke", "static_member_function_pointer"): (
        "resolution::scenario<invoke_static_member_function_pointer>"
    ),
    ("invoke", "member_function_pointer"): (
        "resolution::scenario<invoke_member_function_pointer>"
    ),
    ("invoke", "mutable_lambda"): "resolution::scenario<invoke_mutable_lambda>",
    ("invoke", "generic_lambda_explicit_signature"): (
        "resolution::scenario<invoke_generic_lambda_explicit_signature>"
    ),
    ("invoke", "lvalue_ref_qualified_functor"): (
        "resolution::scenario<invoke_lvalue_ref_qualified_functor>"
    ),
    ("invoke", "rvalue_ref_qualified_functor"): (
        "resolution::scenario<invoke_rvalue_ref_qualified_functor>"
    ),
    ("invoke", "noexcept_functor"): (
        "resolution::scenario<invoke_noexcept_functor>"
    ),
    ("invoke", "move_only_functor"): (
        "resolution::scenario<invoke_move_only_functor>"
    ),
    ("invoke", "multi_argument_callable"): (
        "resolution::scenario<invoke_multi_argument_callable>"
    ),
    ("invoke", "move_only_function"): (
        "resolution::scenario<invoke_move_only_function>"
    ),
    ("factory_override", "factory_function_no_args"): (
        "resolution::member_value_ref<factory_function_value_type, 9>"
    ),
    ("factory_override", "factory_function_with_dependency"): (
        "resolution::member_value_ref<factory_function_dependency_value_type, 39>"
    ),
    ("factory_override", "explicit_constructor_factory"): (
        "resolution::member_value_ref<factory_constructor_value_type, 9>"
    ),
    ("factory_override", "detected_constructor"): (
        "resolution::member_value_ref<factory_detected_constructor_value_type, 27>"
    ),
    ("factory_override", "typedef_constructor"): (
        "resolution::member_value_ref<factory_typedef_constructor_value_type, 33>"
    ),
    ("factory_override", "callable_registration_factory"): (
        "resolution::member_value_ref<factory_callable_value_type, 15>"
    ),
    ("factory_override", "explicit_callable_construct"): (
        "resolution::scenario<explicit_callable_construct_scenario>"
    ),
}

FEATURE_CASES = tuple(
    replace(
        feature_case,
        policy=_FEATURE_CASE_POLICIES.get(
            (feature_case.feature, feature_case.name)
        ),
    )
    for feature_case in FEATURE_CASES
)
