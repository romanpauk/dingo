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


FEATURES = (
    Feature(
        name="construct_invoke",
        requires=frozenset(
            {
                "concrete_binding",
                "constructable_dependency",
                "invokable_dependency",
                "stable_concrete_storage",
                "resolved_concrete",
            }
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
        checks=(
            "auto constructed = container.template construct<dependent_type>();",
            "ASSERT_EQ(constructed.value, 3);",
            "auto invoked = container.invoke([](value_type& dependency) { return dependency.marker(); });",
            "ASSERT_EQ(invoked, 3);",
        ),
    ),
    Feature(
        name="resolve_concrete",
        requires=frozenset({"concrete_binding", "resolved_concrete"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_interface",
        requires=frozenset({"interface_binding", "resolved_interface"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_collection",
        requires=frozenset(
            {"collection_binding", "shared_storage", "resolved_collection"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
        system_headers=("algorithm",),
    ),
    Feature(
        name="resolve_keyed",
        requires=frozenset({"keyed_binding", "shared_storage", "resolved_keyed"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_unique_wrapper",
        requires=frozenset(
            {"interface_binding", "unique_storage", "resolved_unique_wrapper"}
        ),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_wrapper",
        requires=frozenset({"resolved_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="custom_wrappers",
        requires=frozenset({"custom_wrapper_binding", "resolved_custom_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="nested_wrappers",
        requires=frozenset({"nested_wrapper_binding", "resolved_nested_wrapper"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="lifetime_counts",
        requires=frozenset({"lifetime_countable"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="resolve_indexed",
        requires=frozenset(
            {"indexed_binding", "shared_storage", "resolved_indexed"}
        ),
        modes=frozenset({"runtime"}),
        container_requires=frozenset({"indexed_container"}),
    ),
    Feature(
        name="indexed_registration_regressions",
        requires=frozenset({"scenario"}),
        modes=frozenset({"runtime"}),
        container_requires=frozenset({"indexed_regression_container"}),
        support_headers=("matrix/support/indexed_registration.h",),
        checks=("exercise_indexed_registration(container);",),
    ),
    Feature(
        name="resolve_keyed_collection",
        requires=frozenset({"keyed_collection_binding", "resolved_keyed_collection"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        system_headers=("algorithm",),
    ),
    Feature(
        name="construct",
        requires=frozenset({"constructable_dependency"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        checks=(
            "auto constructed = container.template construct<dependent_type>();",
            "ASSERT_EQ(constructed.value, 3);",
        ),
    ),
    Feature(
        name="invoke",
        requires=frozenset({"invokable_dependency"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        system_headers=("functional",),
        checks=(
            "auto invoked = container.invoke([](value_type& dependency) { return dependency.marker(); });",
            "ASSERT_EQ(invoked, 3);",
            "auto explicit_invoked = container.template invoke<int(value_type&)>(overloaded_invoker{});",
            "ASSERT_EQ(explicit_invoked, 45);",
            "auto function_invoked = container.invoke(std::function<int(value_type&)>("
            "[](value_type& dependency) { return dependency.marker() + 48; }));",
            "ASSERT_EQ(function_invoked, 51);",
            "auto free_function_invoked = container.invoke(free_function_invoke);",
            "ASSERT_EQ(free_function_invoked, 57);",
            "auto noexcept_function_invoked = container.invoke(noexcept_function_invoke);",
            "ASSERT_EQ(noexcept_function_invoked, 63);",
            "auto member_function_invoked = container.invoke(member_function_invoker::invoke);",
            "ASSERT_EQ(member_function_invoked, 69);",
            "auto member_pointer_invoked = container.invoke(&value_type::marker);",
            "ASSERT_EQ(member_pointer_invoked, 3);",
            "auto mutable_lambda_invoked = container.invoke([offset = 96](value_type& dependency) mutable {",
            "    return dependency.marker() + offset;",
            "});",
            "ASSERT_EQ(mutable_lambda_invoked, 99);",
            "auto generic_lambda_invoked = container.template invoke<int(value_type&)>("
            "[](auto& dependency) { return dependency.marker() + 102; });",
            "ASSERT_EQ(generic_lambda_invoked, 105);",
            "lvalue_ref_qualified_invoker lvalue_ref_invoker;",
            "auto lvalue_ref_invoked = container.invoke(lvalue_ref_invoker);",
            "ASSERT_EQ(lvalue_ref_invoked, 75);",
            "auto rvalue_ref_invoked = container.invoke(rvalue_ref_qualified_invoker{});",
            "ASSERT_EQ(rvalue_ref_invoked, 81);",
            "auto noexcept_functor_invoked = container.invoke(noexcept_invoker{});",
            "ASSERT_EQ(noexcept_functor_invoked, 87);",
            "auto move_only_invoked = container.invoke(move_only_invoker{});",
            "ASSERT_EQ(move_only_invoked, 93);",
            "auto multi_arg_invoked = container.template invoke<int(value_type&, const value_type&)>("
            "[](value_type& first, const value_type& second) { "
            "return first.marker() + second.marker() + 108; });",
            "ASSERT_EQ(multi_arg_invoked, 114);",
            "#if defined(__cpp_lib_move_only_function)",
            "auto move_only_function_invoked = container.invoke("
            "std::move_only_function<int(value_type&)>("
            "[](value_type& dependency) { return dependency.marker() + 114; }));",
            "ASSERT_EQ(move_only_function_invoked, 117);",
            "#endif",
        ),
    ),
    Feature(
        name="construct_collection",
        requires=frozenset({"collection_binding", "constructable_collection"}),
        modes=frozenset({"runtime", "static", "mixed"}),
        system_headers=("algorithm", "map"),
        checks=(
            "auto elements = container.template construct_collection<std::vector<std::shared_ptr<element_interface>>>();",
            "std::vector<int> ids;",
            "for (auto& element : elements) { ids.push_back(element->id()); }",
            "std::sort(ids.begin(), ids.end());",
            "ASSERT_EQ(ids, (std::vector<int>{0, 1}));",
        ),
    ),
    Feature(
        name="local_bindings",
        requires=frozenset({"local_bindings"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="nested_container",
        requires=frozenset(
            {
                "concrete_binding",
                "direct_value_resolution",
                "nested_container",
                "resolved_concrete",
            }
        ),
        modes=frozenset({"runtime"}),
        system_headers=("type_traits",),
        support_headers=("matrix/support/nested_container.h",),
        checks=(
            "typename std::remove_reference_t<decltype(container)>::allocator_type allocator;",
            "std::remove_reference_t<decltype(container)> parent(allocator);",
            "parent.template register_type<dingo::scope<dingo::external>, dingo::storage<int>>(1);",
            "parent.template register_type<dingo::scope<dingo::unique>, dingo::storage<nested_value_type>>();",
            "parent.template register_type<dingo::scope<dingo::unique>, dingo::storage<nested_parent_value_type>>();",
            "std::remove_reference_t<decltype(container)> child(&parent, allocator);",
            "ASSERT_EQ(child.template resolve<nested_parent_value_type>().value, 1);",
            "child.template register_type<dingo::scope<dingo::external>, dingo::storage<int>>(2);",
            "child.template register_type<dingo::scope<dingo::unique>, dingo::storage<nested_value_type>>();",
            "ASSERT_EQ(parent.template resolve<nested_value_type>().value, 1);",
            "ASSERT_EQ(child.template resolve<nested_value_type>().value, 2);",
        ),
    ),
    Feature(
        name="annotated",
        requires=frozenset({"annotated_binding", "resolved_annotated"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="variant",
        requires=frozenset({"variant_binding", "resolved_variant"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="array",
        requires=frozenset({"array_binding", "resolved_array"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="factory_override",
        requires=frozenset({"factory_override"}),
        modes=frozenset({"runtime", "static", "mixed"}),
    ),
    Feature(
        name="runtime_container_regressions",
        requires=frozenset({"scenario"}),
        modes=frozenset({"runtime"}),
        support_headers=("matrix/support/runtime_regressions.h",),
        checks=(
            "container.template register_type<dingo::scope<dingo::unique>, dingo::storage<std::shared_ptr<unique_hierarchy_s>>>();",
            "container.template register_type<dingo::scope<dingo::unique>, dingo::storage<std::unique_ptr<unique_hierarchy_u>>>();",
            "container.template register_type<dingo::scope<dingo::unique>, dingo::storage<unique_hierarchy_b>>();",
            "container.template resolve<unique_hierarchy_b>();",
            "",
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<std::unique_ptr<nested_wrapper_value>>>, dingo::interfaces<interface_type>>();",
            "container.template register_type<dingo::scope<dingo::unique>, dingo::storage<nested_wrapper_consumer>>();",
            "auto consumer = container.template resolve<nested_wrapper_consumer>();",
            "ASSERT_TRUE(is_constructed_value(consumer.value));",
            "ASSERT_EQ(consumer.pointer, std::addressof(consumer.value));",
            "",
            "clear_rollback_stats();",
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<rollback_a>>();",
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<rollback_b>>();",
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<rollback_c>>();",
            "ASSERT_THROW(container.template resolve<rollback_c&>(), rollback_exception);",
            "ASSERT_EQ(rollback_a::constructor_count, 1u);",
            "ASSERT_EQ(rollback_a::destructor_count, 0u);",
            "ASSERT_EQ(rollback_b::constructor_count, 1u);",
            "ASSERT_EQ(rollback_b::destructor_count, 0u);",
            "container.template resolve<rollback_a&>();",
            "ASSERT_THROW(container.template resolve<rollback_c>(), rollback_exception);",
            "ASSERT_EQ(rollback_a::constructor_count, 1u);",
            "ASSERT_EQ(rollback_a::destructor_count, 0u);",
            "ASSERT_EQ(rollback_b::constructor_count, 1u);",
            "ASSERT_EQ(rollback_b::destructor_count, 0u);",
            "",
            "ASSERT_THROW((container.template register_type<dingo::scope<dingo::shared>, dingo::storage<rollback_a>>()), dingo::type_already_registered_exception);",
        ),
    ),
    Feature(
        name="runtime_construct_dependency_regressions",
        requires=frozenset({"scenario"}),
        modes=frozenset({"runtime"}),
        support_headers=("matrix/support/runtime_regressions.h",),
        checks=(
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<construct_dependency::a>>>();",
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<std::shared_ptr<construct_dependency::b>>>();",
            "ASSERT_THROW(container.template construct<construct_dependency::foo>(), dingo::type_not_found_exception);",
            "auto opted_in = container.template construct<construct_dependency::opted_in_foo>();",
            "EXPECT_TRUE(opted_in.template dependency<std::shared_ptr<construct_dependency::a>>());",
            "EXPECT_TRUE(opted_in.template dependency<std::shared_ptr<construct_dependency::b>>());",
            "auto aggregate = container.template construct<construct_dependency::aggregate_foo>();",
            "EXPECT_TRUE(aggregate.template dependency<std::shared_ptr<construct_dependency::a>>());",
            "EXPECT_TRUE(aggregate.template dependency<std::shared_ptr<construct_dependency::b>>());",
            "ASSERT_THROW(container.template construct<construct_dependency::ambiguous_opted_in_foo>(), dingo::type_not_found_exception);",
        ),
    ),
    Feature(
        name="runtime_exception_regressions",
        requires=frozenset({"scenario"}),
        modes=frozenset({"runtime"}),
        system_headers=("dingo/type/type_name.h",),
        support_headers=("matrix/support/values.h",),
        checks=(
            "struct MissingDependency {};",
            "struct MissingType { explicit MissingType(MissingDependency&) {} };",
            "try {",
            "    (void)container.template resolve<MissingType>();",
            "    FAIL() << \"expected type_not_found_exception\";",
            "} catch (const dingo::type_not_found_exception& e) {",
            "    std::string expected = \"type not found: \";",
            "    expected += type_name<MissingType>();",
            "    ASSERT_STREQ(e.what(), expected.c_str());",
            "}",
            "",
            "struct LeafDependency {};",
            "struct Dependency { explicit Dependency(LeafDependency&) {} };",
            "struct MissingWithDependency { explicit MissingWithDependency(Dependency&) {} };",
            "container.template register_type<dingo::scope<dingo::unique>, dingo::storage<MissingWithDependency>>();",
            "try {",
            "    (void)container.template resolve<MissingWithDependency>();",
            "    FAIL() << \"expected type_not_found_exception\";",
            "} catch (const dingo::type_not_found_exception& e) {",
            "    std::string expected = \"type not found: \";",
            "    expected += type_name<Dependency&>();",
            "    expected += \" (required by \";",
            "    expected += type_name<MissingWithDependency>();",
            "    expected += \")\";",
            "    ASSERT_STREQ(e.what(), expected.c_str());",
            "}",
            "",
            "try {",
            "    (void)container.template construct_collection<std::vector<interface_type*>>();",
            "    FAIL() << \"expected type_not_found_exception\";",
            "} catch (const dingo::type_not_found_exception& e) {",
            "    std::string expected = \"type not found for collection \";",
            "    expected += type_name<std::vector<interface_type*>>();",
            "    expected += \" (element type: \";",
            "    expected += type_name<interface_type*>();",
            "    expected += \")\";",
            "    ASSERT_STREQ(e.what(), expected.c_str());",
            "}",
        ),
    ),
    Feature(
        name="runtime_unique_reference_regressions",
        requires=frozenset({"scenario"}),
        modes=frozenset({"runtime"}),
        support_headers=("matrix/support/runtime_regressions.h",),
        checks=(
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<shared_from_unique_reference>>();",
            "container.template register_type<dingo::scope<dingo::unique>, dingo::storage<unique_reference_value>, dingo::factory<dingo::constructor<unique_reference_value()>>>();",
            "auto& shared = container.template resolve<shared_from_unique_reference&>();",
            "ASSERT_FALSE(shared.token.expired());",
            "ASSERT_FALSE(container.template resolve<shared_from_unique_reference&>().token.expired());",
            "",
            "int destructor_count = 0;",
            "container.template register_type<dingo::scope<dingo::external>, dingo::storage<int&>>(destructor_count);",
            "container.template register_type<dingo::scope<dingo::unique>, dingo::storage<unique_reference_exception_value>>();",
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<shared_unique_reference_exception_value>>();",
            "ASSERT_THROW(container.template resolve<shared_unique_reference_exception_value>(), std::runtime_error);",
            "ASSERT_EQ(destructor_count, 2);",
        ),
    ),
    Feature(
        name="shared_cyclical_regressions",
        requires=frozenset({"scenario"}),
        modes=frozenset({"runtime"}),
        support_headers=("matrix/support/runtime_regressions.h",),
        checks=(
            "cyclical_rollback_b::fail = true;",
            "container.template register_type<dingo::scope<dingo::shared_cyclical>, dingo::storage<std::shared_ptr<cyclical_rollback_a>>>();",
            "container.template register_type<dingo::scope<dingo::shared_cyclical>, dingo::storage<std::shared_ptr<cyclical_rollback_b>>, dingo::interfaces<cyclical_rollback_b, interface_type>>();",
            "ASSERT_THROW(container.template resolve<cyclical_rollback_a&>(), cyclical_rollback_exception);",
            "auto& rollback_a = container.template resolve<cyclical_rollback_a&>();",
            "auto& rollback_b = container.template resolve<cyclical_rollback_b&>();",
            "ASSERT_EQ(static_cast<interface_type*>(&rollback_b), rollback_a.dependency.get());",
            "",
            "container.template register_type<dingo::scope<dingo::shared>, dingo::storage<cyclical_commit_c>>();",
            "ASSERT_THROW(container.template resolve<cyclical_commit_c&>(), cyclical_rollback_exception);",
            "ASSERT_EQ(static_cast<interface_type*>(&rollback_b), rollback_a.dependency.get());",
        ),
    ),
    Feature(
        name="mixed_external_dependency",
        requires=frozenset(
            {"mixed_external_dependency", "resolved_mixed_external_dependency"}
        ),
        modes=frozenset({"mixed"}),
    ),
    Feature(
        name="mixed_singular_ambiguity",
        requires=frozenset(
            {"mixed_singular_conflict", "resolved_mixed_singular_conflict"}
        ),
        modes=frozenset({"mixed"}),
        checks=(
            "ASSERT_THROW(container.template resolve<value_type&>(), dingo::type_ambiguous_exception);",
        ),
    ),
    Feature(
        name="custom_allocator",
        requires=frozenset(
            {
                "custom_allocator_container",
                "concrete_binding",
                "direct_value_resolution",
                "stable_concrete_storage",
            }
        ),
        modes=frozenset({"runtime"}),
        checks=(
            "ASSERT_GT(test_allocator_stats<void>::allocated(), 0u);",
            "auto& resolved = container.template resolve<value_type&>();",
            "ASSERT_TRUE(is_constructed_value(resolved));",
        ),
    ),
    Feature(
        name="custom_rtti",
        requires=frozenset(
            {
                "custom_rtti_container",
                "concrete_binding",
                "direct_value_resolution",
                "stable_concrete_storage",
            }
        ),
        modes=frozenset({"runtime"}),
        system_headers=("type_traits",),
        checks=(
            "using rtti_type = typename std::remove_reference_t<decltype(container)>::rtti_type;",
            "static_assert(std::is_same_v<rtti_type, dingo::rtti<dingo::static_provider>>);",
            "auto& resolved = container.template resolve<value_type&>();",
            "ASSERT_TRUE(is_constructed_value(resolved));",
        ),
    ),
)
