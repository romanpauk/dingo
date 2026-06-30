#!/usr/bin/env python3
#
# This file is part of dingo project <https://github.com/romanpauk/dingo>
#
# See LICENSE for license and copyright information
# SPDX-License-Identifier: MIT
#

from __future__ import annotations

from schema import (
    ContainerSpec,
    ExposedType,
    Feature,
    RegistrationMode,
    RegistrationSpec,
    ResolvedType,
    ScopeSpec,
    StoredType,
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
        provides=frozenset({"resolved_concrete", "constructable_dependency", "invokable_dependency"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "value_type& instance = container.template resolve<value_type&>();",
                    "value_type* pointer = container.template resolve<value_type*>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                    "ASSERT_TRUE(is_constructed_value(*pointer));",
                    "ASSERT_EQ(&instance, pointer);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_concrete_value"}),
        requires=frozenset({"direct_value_resolution"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "auto instance = container.template resolve<value_type>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_rvalue",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete"}),
        requires=frozenset({"unique_storage", "stored_value"}),
        checks=(
            (
                "lifetime_counts",
                (
                    "value_type&& instance = container.template resolve<value_type&&>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="cycle_ref",
        supported_exposed_types=frozenset({"cycle_concrete"}),
        provides=frozenset({"resolved_concrete"}),
        requires=frozenset({"cycle_graph"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "auto& instance = container.template resolve<cycle_a_type&>();",
                    "ASSERT_NE(instance.dependency, nullptr);",
                    "ASSERT_EQ(instance.dependency->dependency, &instance);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="dependent_type_ref",
        supported_exposed_types=frozenset({"mixed_external_dependency"}),
        provides=frozenset({"resolved_mixed_external_dependency"}),
        checks=(
            (
                "mixed_external_dependency",
                (
                    "auto& resolved = container.template resolve<dependent_type&>();",
                    "ASSERT_EQ(resolved.value, 3);",
                ),
            ),
        ),
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
        checks=(
            (
                "resolve_concrete",
                (
                    "const value_type& instance = container.template resolve<const value_type&>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="const_value_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_concrete", "resolved_const_concrete"}),
        requires=frozenset({"stable_concrete_storage", "direct_value_resolution"}),
        checks=(
            (
                "resolve_concrete",
                (
                    "const value_type* instance = container.template resolve<const value_type*>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_value",
        supported_exposed_types=frozenset({"copyable_interface_type"}),
        provides=frozenset({"resolved_interface", "resolved_interface_value"}),
        checks=(
            (
                "resolve_interface",
                (
                    "auto instance = container.template resolve<copyable_interface_type>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_ref_ptr_shared_ptr",
        supported_exposed_types=frozenset({"interface_type"}),
        provides=frozenset({"resolved_interface", "resolved_shared_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "interface_type& instance = container.template resolve<interface_type&>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                    "interface_type* pointer = container.template resolve<interface_type*>();",
                    "ASSERT_TRUE(is_constructed_value(*pointer));",
                    "auto& handle = container.template resolve<std::shared_ptr<interface_type>&>();",
                    "ASSERT_TRUE(is_constructed_value(*handle));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_ref",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "interface_type& instance = container.template resolve<interface_type&>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_ptr",
        supported_exposed_types=frozenset({"interface_type", "multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "interface_type* instance = container.template resolve<interface_type*>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="second_interface_ref",
        supported_exposed_types=frozenset({"multiple_interfaces"}),
        provides=frozenset({"resolved_interface"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_interface",
                (
                    "second_interface_type& instance = container.template resolve<second_interface_type&>();",
                    "ASSERT_EQ(instance.second_marker(), 4);",
                    "ASSERT_TRUE(instance.valid());",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="unique_interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface_type"}),
        provides=frozenset({"resolved_unique_wrapper"}),
        requires=frozenset({"unique_storage"}),
        checks=(
            (
                "resolve_unique_wrapper",
                (
                    "auto instance = container.template resolve<std::unique_ptr<unique_interface_type>&&>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_unique_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_unique_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::unique_ptr<value_type>&&>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_unique_ptr",
        supported_exposed_types=frozenset({"unique_interface_type"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_unique_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::unique_ptr<unique_interface_type>&&>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_shared_ptr",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::shared_ptr<value_type>>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_shared_ptr_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto& instance = container.template resolve<std::shared_ptr<value_type>&>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_optional",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"unique_storage", "stored_optional"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::optional<value_type>>();",
                    "ASSERT_TRUE(instance.has_value());",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="value_optional_ref",
        supported_exposed_types=frozenset({"concrete"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_optional"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto& instance = container.template resolve<std::optional<value_type>&>();",
                    "ASSERT_TRUE(instance.has_value());",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
            (
                "lifetime_counts",
                (
                    "auto& instance = container.template resolve<std::optional<value_type>&>();",
                    "ASSERT_TRUE(instance.has_value());",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="interface_shared_ptr",
        supported_exposed_types=frozenset({"interface_type"}),
        provides=frozenset({"resolved_wrapper"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "resolve_wrapper",
                (
                    "auto instance = container.template resolve<std::shared_ptr<interface_type>>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="custom_shared_concrete",
        supported_exposed_types=frozenset({"custom_concrete"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"stable_concrete_storage", "stored_custom_shared"}),
        checks=(
            (
                "custom_wrappers",
                (
                    "auto& wrapper = container.template resolve<dingo::test_shared<value_type>&>();",
                    "ASSERT_NE(wrapper.get(), nullptr);",
                    "value_type* pointer = container.template resolve<value_type*>();",
                    "ASSERT_EQ(pointer, wrapper.get());",
                    "ASSERT_TRUE(is_constructed_value(*pointer));",
                    "auto copy = container.template resolve<dingo::test_shared<value_type>>();",
                    "ASSERT_EQ(copy.get(), wrapper.get());",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="custom_shared_interface",
        supported_exposed_types=frozenset({"custom_interface"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"shared_storage", "stored_custom_shared"}),
        checks=(
            (
                "custom_wrappers",
                (
                    "auto& wrapper = container.template resolve<dingo::test_shared<interface_type>&>();",
                    "ASSERT_NE(wrapper.get(), nullptr);",
                    "interface_type* pointer = container.template resolve<interface_type*>();",
                    "ASSERT_EQ(pointer, wrapper.get());",
                    "ASSERT_TRUE(is_constructed_value(*pointer));",
                    "auto copy = container.template resolve<dingo::test_shared<interface_type>>();",
                    "ASSERT_EQ(copy.get(), wrapper.get());",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="custom_unique_interface",
        supported_exposed_types=frozenset({"custom_unique_interface"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"unique_storage", "stored_custom_unique"}),
        checks=(
            (
                "custom_wrappers",
                (
                    "auto unique = container.template resolve<dingo::test_unique<unique_interface_type>>();",
                    "ASSERT_NE(unique.get(), nullptr);",
                    "ASSERT_TRUE(is_constructed_value(*unique.get()));",
                    "auto shared = container.template resolve<dingo::test_shared<unique_interface_type>>();",
                    "ASSERT_NE(shared.get(), nullptr);",
                    "ASSERT_TRUE(is_constructed_value(*shared.get()));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="custom_optional_ref",
        supported_exposed_types=frozenset({"custom_concrete"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"stable_concrete_storage", "stored_custom_optional"}),
        checks=(
            (
                "custom_wrappers",
                (
                    "auto& wrapper = container.template resolve<dingo::test_optional<value_type>&>();",
                    "ASSERT_NE(wrapper.get(), nullptr);",
                    "ASSERT_TRUE(is_constructed_value(*wrapper.get()));",
                    "value_type& value = container.template resolve<value_type&>();",
                    "ASSERT_EQ(std::addressof(value), wrapper.get());",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="custom_optional_value",
        supported_exposed_types=frozenset({"custom_concrete"}),
        provides=frozenset({"resolved_custom_wrapper"}),
        requires=frozenset({"unique_storage", "stored_custom_optional"}),
        checks=(
            (
                "custom_wrappers",
                (
                    "auto wrapper = container.template resolve<dingo::test_optional<value_type>>();",
                    "ASSERT_NE(wrapper.get(), nullptr);",
                    "ASSERT_TRUE(is_constructed_value(*wrapper.get()));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_vector",
        supported_exposed_types=frozenset({"element_collection"}),
        provides=frozenset({"resolved_collection", "constructable_collection"}),
        checks=(
            (
                "resolve_collection",
                (
                    "auto elements = container.template resolve<std::vector<std::shared_ptr<element_interface>>>();",
                    "std::vector<int> ids;",
                    "for (auto& element : elements) { ids.push_back(element->id()); }",
                    "std::sort(ids.begin(), ids.end());",
                    "ASSERT_EQ(ids, (std::vector<int>{0, 1}));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_map_custom_insert",
        supported_exposed_types=frozenset({"element_collection"}),
        provides=frozenset({"constructable_collection"}),
        checks=(
            (
                "construct_collection",
                (
                    "auto elements = container.template construct_collection<std::map<int, std::shared_ptr<element_interface>>>(",
                    "    [](auto& collection, auto&& value) { collection.emplace(value->id(), std::move(value)); });",
                    "ASSERT_EQ(elements.size(), 2u);",
                    "ASSERT_EQ(elements.at(0)->id(), 0);",
                    "ASSERT_EQ(elements.at(1)->id(), 1);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_keyed_shared_ptr_ref",
        supported_exposed_types=frozenset({"element_keyed"}),
        provides=frozenset({"resolved_keyed"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto& first = container.template resolve<std::shared_ptr<element_interface>&>(dingo::key<key_a>{});",
                    "auto& second = container.template resolve<std::shared_ptr<element_interface>&>(dingo::key<key_b>{});",
                    "ASSERT_EQ(first->id(), 0);",
                    "ASSERT_EQ(second->id(), 1);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_value_dependency",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"constructable_dependency", "invokable_dependency"}),
        requires=frozenset({"direct_value_resolution", "stable_concrete_storage"}),
        checks=(
            (
                "construct",
                (
                    "auto constructed = container.template construct<keyed_value_dependency_type>();",
                    "ASSERT_EQ(constructed.value, 3);",
                ),
            ),
            (
                "invoke",
                (
                    "auto invoked = container.invoke([](dingo::query<value_type&, dingo::key<key_a>> dependency) {",
                    "    return static_cast<value_type&>(dependency).marker();",
                    "});",
                    "ASSERT_EQ(invoked, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_value",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"direct_value_resolution"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto instance = container.template resolve<value_type>(dingo::key<key_a>{});",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_value_ref",
        supported_exposed_types=frozenset({"keyed_concrete"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"direct_value_resolution", "stable_concrete_storage"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto& instance = container.template resolve<value_type&>(dingo::key<key_a>{});",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_interface_ref",
        supported_exposed_types=frozenset({"keyed_interface"}),
        provides=frozenset({"resolved_keyed"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "resolve_keyed",
                (
                    "auto& instance = container.template resolve<interface_type&>(dingo::key<key_a>{});",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_keyed_vector",
        supported_exposed_types=frozenset({"element_keyed_collection"}),
        provides=frozenset({"resolved_keyed_collection"}),
        checks=(
            (
                "resolve_keyed_collection",
                (
                    "auto elements = container.template resolve<std::vector<std::shared_ptr<element_interface>>>(dingo::key<key_a>{});",
                    "std::vector<int> ids;",
                    "for (auto& element : elements) { ids.push_back(element->id()); }",
                    "std::sort(ids.begin(), ids.end());",
                    "ASSERT_EQ(ids, (std::vector<int>{0, 1}));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="keyed_collection_dependency",
        supported_exposed_types=frozenset({"element_keyed_collection"}),
        provides=frozenset({"constructable_dependency", "invokable_dependency"}),
        checks=(
            (
                "construct",
                (
                    "auto constructed = container.template construct<keyed_collection_dependency_type>();",
                    "ASSERT_EQ(constructed.count, 2u);",
                    "ASSERT_EQ(constructed.sum, 1);",
                ),
            ),
            (
                "invoke",
                (
                    "auto invoked = container.invoke([](dingo::query<std::vector<std::shared_ptr<element_interface>>, dingo::key<key_a>> elements) {",
                    "    auto values = static_cast<std::vector<std::shared_ptr<element_interface>>>(elements);",
                    "    return values.size();",
                    "});",
                    "ASSERT_EQ(invoked, 2u);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="element_indexed_shared_ptr",
        supported_exposed_types=frozenset({"element_indexed"}),
        provides=frozenset({"resolved_indexed"}),
        supported_modes=frozenset({"runtime"}),
        checks=(
            (
                "resolve_indexed",
                (
                    "auto first = container.template resolve<std::shared_ptr<element_interface>>(std::size_t{0});",
                    "auto second = container.template resolve<std::shared_ptr<element_interface>>(std::size_t{1});",
                    "ASSERT_EQ(first->id(), 0);",
                    "ASSERT_EQ(second->id(), 1);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_array_ptr",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
        checks=(
            (
                "array",
                (
                    "auto* values = container.template resolve<value_type*>();",
                    "ASSERT_TRUE(is_constructed_value(values[0]));",
                    "ASSERT_TRUE(is_constructed_value(values[1]));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_array_ref",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
        checks=(
            (
                "array",
                (
                    "auto& values = container.template resolve<value_type(&)[2]>();",
                    "ASSERT_TRUE(is_constructed_value(values[0]));",
                    "ASSERT_TRUE(is_constructed_value(values[1]));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_array_ptr_to_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_1d"}),
        checks=(
            (
                "array",
                (
                    "auto values = container.template resolve<value_type(*)[2]>();",
                    "ASSERT_TRUE(is_constructed_value((*values)[0]));",
                    "ASSERT_TRUE(is_constructed_value((*values)[1]));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="raw_nd_array_ref",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"array_2d"}),
        checks=(
            (
                "array",
                (
                    "auto& values = container.template resolve<value_type(&)[2][3]>();",
                    "ASSERT_TRUE(is_constructed_value(values[0][0]));",
                    "ASSERT_TRUE(is_constructed_value(values[1][2]));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="unique_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"unique_storage", "array_smart_unique"}),
        checks=(
            (
                "array",
                (
                    "auto values = container.template resolve<std::unique_ptr<value_type[]>&&>();",
                    "ASSERT_TRUE(is_constructed_value(values[0]));",
                    "ASSERT_TRUE(is_constructed_value(values[1]));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="shared_array",
        supported_exposed_types=frozenset({"array_concrete"}),
        provides=frozenset({"resolved_array"}),
        requires=frozenset({"shared_storage", "array_smart_shared"}),
        checks=(
            (
                "array",
                (
                    "auto values = container.template resolve<std::shared_ptr<value_type[]>>();",
                    "ASSERT_TRUE(is_constructed_value(values[0]));",
                    "ASSERT_TRUE(is_constructed_value(values[1]));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="shared_unique_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_shared_unique"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto& instance = container.template resolve<shared_unique_value_type&>();",
                    "ASSERT_TRUE(instance);",
                    "ASSERT_TRUE(*instance);",
                    "ASSERT_TRUE(is_constructed_value(**instance));",
                    "auto handle = container.template resolve<shared_unique_value_type>();",
                    "ASSERT_TRUE(handle);",
                    "ASSERT_TRUE(*handle);",
                    "ASSERT_TRUE(is_constructed_value(**handle));",
                    "auto& unique_handle = container.template resolve<std::unique_ptr<value_type>&>();",
                    "ASSERT_TRUE(unique_handle);",
                    "ASSERT_TRUE(is_constructed_value(*unique_handle));",
                    "auto& leaf = container.template resolve<value_type&>();",
                    "ASSERT_TRUE(is_constructed_value(leaf));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="shared_unique_array_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_shared_unique_array"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto& instance = container.template resolve<shared_unique_array_value_type&>();",
                    "ASSERT_TRUE(instance);",
                    "ASSERT_TRUE(*instance);",
                    "ASSERT_TRUE(is_constructed_value((*instance)[0]));",
                    "ASSERT_TRUE(is_constructed_value((*instance)[1]));",
                    "auto handle = container.template resolve<shared_unique_array_value_type>();",
                    "ASSERT_TRUE(handle);",
                    "ASSERT_TRUE(*handle);",
                    "ASSERT_TRUE(is_constructed_value((*handle)[0]));",
                    "auto& unique_array = container.template resolve<std::unique_ptr<value_type[]>&>();",
                    "ASSERT_TRUE(unique_array);",
                    "ASSERT_TRUE(is_constructed_value(unique_array[0]));",
                    "auto* first = container.template resolve<value_type*>();",
                    "ASSERT_TRUE(is_constructed_value(*first));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="unique_shared_value",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_unique_shared"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto instance = container.template resolve<unique_shared_value_type&&>();",
                    "ASSERT_TRUE(instance);",
                    "ASSERT_TRUE(*instance);",
                    "ASSERT_TRUE(is_constructed_value(**instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_unique_value",
        supported_exposed_types=frozenset({"variant_unique_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_variant_unique"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto instance = container.template resolve<variant_unique_value_type>();",
                    "ASSERT_TRUE(std::holds_alternative<std::unique_ptr<value_type>>(instance));",
                    "auto& value = std::get<std::unique_ptr<value_type>>(instance);",
                    "ASSERT_TRUE(value);",
                    "ASSERT_TRUE(is_constructed_value(*value));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_shared_value",
        supported_exposed_types=frozenset({"variant_shared_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_variant_shared"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto& instance = container.template resolve<variant_shared_value_type&>();",
                    "ASSERT_TRUE(std::holds_alternative<std::shared_ptr<value_type>>(instance));",
                    "auto value = std::get<std::shared_ptr<value_type>>(instance);",
                    "ASSERT_TRUE(value);",
                    "ASSERT_TRUE(is_constructed_value(*value));",
                    "auto alternative = container.template resolve<std::shared_ptr<value_type>>();",
                    "ASSERT_TRUE(alternative);",
                    "ASSERT_TRUE(is_constructed_value(*alternative));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_shared_unique_value",
        supported_exposed_types=frozenset({"variant_shared_unique_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_variant_shared_unique"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto& instance = container.template resolve<variant_shared_unique_value_type&>();",
                    "using handle_type = std::shared_ptr<std::unique_ptr<value_type>>;",
                    "ASSERT_TRUE((std::holds_alternative<handle_type>(instance)));",
                    "auto handle = std::get<handle_type>(instance);",
                    "ASSERT_TRUE(handle);",
                    "ASSERT_TRUE(*handle);",
                    "ASSERT_TRUE(is_constructed_value(**handle));",
                    "auto resolved_handle = container.template resolve<handle_type>();",
                    "ASSERT_TRUE(resolved_handle);",
                    "ASSERT_TRUE(*resolved_handle);",
                    "ASSERT_TRUE(is_constructed_value(**resolved_handle));",
                    "auto& unique_handle = container.template resolve<std::unique_ptr<value_type>&>();",
                    "ASSERT_TRUE(unique_handle);",
                    "ASSERT_TRUE(is_constructed_value(*unique_handle));",
                    "auto& leaf = container.template resolve<value_type&>();",
                    "ASSERT_TRUE(is_constructed_value(leaf));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="unique_variant_value",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_unique_variant"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto instance = container.template resolve<unique_variant_value_type&&>();",
                    "ASSERT_TRUE(instance);",
                    "ASSERT_TRUE(std::holds_alternative<nested_variant_a>(*instance));",
                    "ASSERT_EQ(std::get<nested_variant_a>(*instance).value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="shared_variant_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_shared_variant"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto& instance = container.template resolve<shared_variant_value_type&>();",
                    "ASSERT_TRUE(instance);",
                    "ASSERT_TRUE(std::holds_alternative<nested_variant_a>(*instance));",
                    "auto& alternative = container.template resolve<nested_variant_a&>();",
                    "ASSERT_EQ(alternative.value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="array_variant_value_ref",
        supported_exposed_types=frozenset({"nested_wrapper"}),
        provides=frozenset({"resolved_nested_wrapper"}),
        requires=frozenset({"stored_array_variant"}),
        checks=(
            (
                "nested_wrappers",
                (
                    "auto& instance = container.template resolve<array_variant_value_type&>();",
                    "ASSERT_TRUE((std::holds_alternative<std::array<value_type, 2>>(instance)));",
                    "auto& values = std::get<std::array<value_type, 2>>(instance);",
                    "ASSERT_TRUE(is_constructed_value(values[0]));",
                    "ASSERT_TRUE(is_constructed_value(values[1]));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_value",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant"}),
        checks=(
            (
                "variant",
                (
                    "auto instance = container.template resolve<std::variant<variant_a, variant_b>>();",
                    "ASSERT_TRUE(std::holds_alternative<variant_a>(instance));",
                    "ASSERT_EQ(std::get<variant_a>(instance).value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_alternative_value",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        checks=(
            (
                "variant",
                (
                    "auto instance = container.template resolve<variant_a>();",
                    "ASSERT_EQ(instance.value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_alternative_ref",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "variant",
                (
                    "auto& instance = container.template resolve<variant_a&>();",
                    "ASSERT_EQ(instance.value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="variant_alternative_ptr",
        supported_exposed_types=frozenset({"variant_concrete"}),
        provides=frozenset({"resolved_variant", "resolved_variant_alternative"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "variant",
                (
                    "auto* instance = container.template resolve<variant_a*>();",
                    "ASSERT_EQ(instance->value, 3);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_value_ref",
        supported_exposed_types=frozenset({"annotated_concrete"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"stable_concrete_storage"}),
        checks=(
            (
                "annotated",
                (
                    "auto& instance = container.template resolve<dingo::annotated<value_type&, tag_a>>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_ref",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "annotated",
                (
                    "auto& instance = container.template resolve<dingo::annotated<interface_type&, tag_a>>();",
                    "ASSERT_TRUE(is_constructed_value(instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_ptr",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage"}),
        checks=(
            (
                "annotated",
                (
                    "auto instance = container.template resolve<dingo::annotated<interface_type*, tag_a>>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_shared_ptr",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "annotated",
                (
                    "auto instance = container.template resolve<dingo::annotated<std::shared_ptr<interface_type>, tag_a>>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="annotated_interface_shared_ptr_ref",
        supported_exposed_types=frozenset({"annotated_interface"}),
        provides=frozenset({"resolved_annotated"}),
        requires=frozenset({"shared_storage", "stored_shared_ptr"}),
        checks=(
            (
                "annotated",
                (
                    "auto& instance = container.template resolve<dingo::annotated<std::shared_ptr<interface_type>&, tag_a>>();",
                    "ASSERT_TRUE(is_constructed_value(*instance));",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="local_value_ref",
        supported_exposed_types=frozenset({"local_value"}),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "local_bindings",
                (
                    "auto& resolved = container.template resolve<local_value_type&>();",
                    "ASSERT_EQ(resolved.value, 12);",
                    "ASSERT_EQ(container.template construct<local_value_type>().value, 12);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="local_override_value_ref",
        supported_exposed_types=frozenset({"local_override_value"}),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "local_bindings",
                (
                    "auto& resolved = container.template resolve<local_override_value_type&>();",
                    "ASSERT_EQ(resolved.value, 4);",
                    "ASSERT_EQ(container.template construct<local_override_value_type>().value, 4);",
                ),
            ),
        ),
    ),
    ResolvedType(
        name="local_collection_value_ref",
        supported_exposed_types=frozenset(
            {"local_collection_value", "local_collection_runtime_host_value"}
        ),
        provides=frozenset({"resolved_concrete"}),
        checks=(
            (
                "local_bindings",
                (
                    "auto& resolved = container.template resolve<local_collection_value_type&>();",
                    "ASSERT_EQ(resolved.count, 2u);",
                    "ASSERT_EQ(resolved.sum, 1);",
                    "auto constructed = container.template construct<local_collection_value_type>();",
                    "ASSERT_EQ(constructed.count, 2u);",
                    "ASSERT_EQ(constructed.sum, 1);",
                ),
            ),
        ),
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
