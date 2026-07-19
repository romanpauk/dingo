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


FEATURE_CASES = (
    FeatureCaseSpec(
        name="native_parent_container_shape",
        feature="nested_container",
        headers=("matrix/scenarios/parent/native.h",),
    ),
    FeatureCaseSpec(
        name="factory_function_no_args",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_function_value"}),
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
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(),
        ),
    ),
    FeatureCaseSpec(
        name="typedef_constructor",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_typedef_constructor_value"}),
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
            RegistrationSpec(),
        ),
    ),
    FeatureCaseSpec(
        name="callable_registration_factory",
        feature="factory_override",
        supported_exposed_types=frozenset({"factory_callable_value"}),
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
        registrations=(
            RegistrationSpec(storage="dingo::storage<value_type>"),
        ),
    ),
)


_FEATURE_CASE_POLICIES = {
    ("nested_container", "native_parent_container_shape"): (
        "resolution::scenario<native_parent_container_scenario>"
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
