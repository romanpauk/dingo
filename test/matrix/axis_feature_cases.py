#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from schema import FeatureCaseSpec, RegistrationSpec


VALUE_DEPENDENCY = frozenset({"direct_value_resolution", "stable_concrete_storage"})


FEATURE_CASES = (
    FeatureCaseSpec(
        name="container_child_container_parent",
        feature="static_parent_container",
        requires=frozenset({"static_parent_container_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        checks=(
            "exercise_static_parent_container_pair<container_parent_shape, container_child_shape>();",
        ),
    ),
    FeatureCaseSpec(
        name="container_child_static_parent",
        feature="static_parent_container",
        requires=frozenset({"static_parent_container_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        checks=(
            "exercise_static_parent_container_pair<static_parent_shape, container_child_shape>();",
        ),
    ),
    FeatureCaseSpec(
        name="static_child_static_parent",
        feature="static_parent_container",
        requires=frozenset({"static_parent_static_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        checks=(
            "exercise_static_parent_container_pair<static_parent_shape, static_child_shape>();",
        ),
    ),
    FeatureCaseSpec(
        name="static_child_container_parent",
        feature="static_parent_container",
        requires=frozenset({"static_parent_static_child"}),
        supported_exposed_types=frozenset({"concrete"}),
        supported_resolved_types=frozenset({"value_ref_ptr"}),
        checks=(
            "exercise_static_parent_container_pair<container_parent_shape, static_child_shape>();",
        ),
    ),
    FeatureCaseSpec(
        name="inferred_lambda",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke([](value_type& dependency) { return dependency.marker(); });",
            "ASSERT_EQ(invoked, 3);",
        ),
    ),
    FeatureCaseSpec(
        name="explicit_signature_overload",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.template invoke<int(value_type&)>(overloaded_invoker{});",
            "ASSERT_EQ(invoked, 45);",
        ),
    ),
    FeatureCaseSpec(
        name="std_function",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        system_headers=("functional",),
        checks=(
            "auto invoked = container.invoke(std::function<int(value_type&)>("
            "[](value_type& dependency) { return dependency.marker() + 48; }));",
            "ASSERT_EQ(invoked, 51);",
        ),
    ),
    FeatureCaseSpec(
        name="free_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke(free_function_invoke);",
            "ASSERT_EQ(invoked, 57);",
        ),
    ),
    FeatureCaseSpec(
        name="noexcept_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke(noexcept_function_invoke);",
            "ASSERT_EQ(invoked, 63);",
        ),
    ),
    FeatureCaseSpec(
        name="static_member_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke(member_function_invoker::invoke);",
            "ASSERT_EQ(invoked, 69);",
        ),
    ),
    FeatureCaseSpec(
        name="member_function_pointer",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke(&value_type::marker);",
            "ASSERT_EQ(invoked, 3);",
        ),
    ),
    FeatureCaseSpec(
        name="mutable_lambda",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke([offset = 96](value_type& dependency) mutable {",
            "    return dependency.marker() + offset;",
            "});",
            "ASSERT_EQ(invoked, 99);",
        ),
    ),
    FeatureCaseSpec(
        name="generic_lambda_explicit_signature",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.template invoke<int(value_type&)>("
            "[](auto& dependency) { return dependency.marker() + 102; });",
            "ASSERT_EQ(invoked, 105);",
        ),
    ),
    FeatureCaseSpec(
        name="lvalue_ref_qualified_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "lvalue_ref_qualified_invoker invoker;",
            "auto invoked = container.invoke(invoker);",
            "ASSERT_EQ(invoked, 75);",
        ),
    ),
    FeatureCaseSpec(
        name="rvalue_ref_qualified_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke(rvalue_ref_qualified_invoker{});",
            "ASSERT_EQ(invoked, 81);",
        ),
    ),
    FeatureCaseSpec(
        name="noexcept_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke(noexcept_invoker{});",
            "ASSERT_EQ(invoked, 87);",
        ),
    ),
    FeatureCaseSpec(
        name="move_only_functor",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.invoke(move_only_invoker{});",
            "ASSERT_EQ(invoked, 93);",
        ),
    ),
    FeatureCaseSpec(
        name="multi_argument_callable",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        checks=(
            "auto invoked = container.template invoke<int(value_type&, const value_type&)>("
            "[](value_type& first, const value_type& second) { "
            "return first.marker() + second.marker() + 108; });",
            "ASSERT_EQ(invoked, 114);",
        ),
    ),
    FeatureCaseSpec(
        name="move_only_function",
        feature="invoke",
        requires=VALUE_DEPENDENCY,
        system_headers=("functional",),
        checks=(
            "#if defined(__cpp_lib_move_only_function)",
            "auto invoked = container.invoke(std::move_only_function<int(value_type&)>("
            "[](value_type& dependency) { return dependency.marker() + 114; }));",
            "ASSERT_EQ(invoked, 117);",
            "#endif",
        ),
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
        checks=(
            "auto& resolved = container.template resolve<factory_function_value_type&>();",
            "ASSERT_EQ(resolved.value, 9);",
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
        checks=(
            "auto& resolved = container.template resolve<factory_function_dependency_value_type&>();",
            "ASSERT_EQ(resolved.value, 39);",
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
        checks=(
            "auto& resolved = container.template resolve<factory_constructor_value_type&>();",
            "ASSERT_EQ(resolved.value, 9);",
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
        checks=(
            "auto& resolved = container.template resolve<factory_detected_constructor_value_type&>();",
            "ASSERT_EQ(resolved.value, 27);",
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
        checks=(
            "auto& resolved = container.template resolve<factory_typedef_constructor_value_type&>();",
            "ASSERT_EQ(resolved.value, 33);",
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
                mixed="runtime",
                static=False,
            ),
        ),
        checks=(
            "auto& resolved = container.template resolve<factory_callable_value_type&>();",
            "ASSERT_EQ(resolved.value, 15);",
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
        checks=(
            "auto constructed = container.template construct<factory_callable_value_type>("
            "dingo::callable<factory_callable_value_type(value_type&)>("
            "factory_overloaded_callable{}));",
            "ASSERT_EQ(constructed.value, 21);",
        ),
    ),
)
