//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/resolution/conversion_cache.h>
#include <dingo/resolution/instance_resolver.h>
#include <dingo/resolution/interface_storage_traits.h>
#include <dingo/type/rebind_type.h>
#include <dingo/rtti/rtti.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/registration/type_registration.h>

#include <gtest/gtest.h>

using namespace dingo;

TEST(type_registration_test, get_type) {
    struct A {};
    using list = type_list<storage<A>, factory<A>, interfaces<A, A>>;
    using storage_type = detail::get_type_t<storage<void>, list>;
    static_assert(std::is_same_v<storage_type, storage<A>>);

    using factory_type = detail::get_type_t<factory<void>, list>;
    static_assert(std::is_same_v<factory_type, factory<A>>);

    using interface_type = detail::get_type_t<interfaces<void>, list>;
    static_assert(std::is_same_v<interface_type, interfaces<A, A>>);
}

TEST(type_registration_test, registration_basic) {
    using registration =
        type_registration<storage<int>, interfaces<double, float>, factory<int>,
                          scope<int>>;

    static_assert(
        std::is_same_v<typename registration::storage_type, storage<int>>);
    static_assert(std::is_same_v<typename registration::interface_type,
                                 interfaces<double, float>>);
    static_assert(
        std::is_same_v<typename registration::factory_type, factory<int>>);
    static_assert(
        std::is_same_v<typename registration::scope_type, scope<int>>);
}

TEST(type_registration_test, registration_deduction) {
    static_assert(
        std::is_same_v<
            typename type_registration<scope<int>, factory<int>>::storage_type,
            storage<int>>);
    static_assert(
        std::is_same_v<typename type_registration<scope<int>,
                                                  factory<int>>::interface_type,
                       interfaces<int>>);

    static_assert(
        std::is_same_v<typename type_registration<scope<int>,
                                                  storage<int>>::interface_type,
                       interfaces<int>>);
    static_assert(
        std::is_same_v<
            typename type_registration<scope<int>, storage<int>>::factory_type,
            factory<constructor_detection<int>>>);
}

TEST(type_registration_test, registration_specialization) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};

    using shared_registration = type_registration<scope<shared>, storage<A>>;
    using shared_storage = detail::storage<
        typename shared_registration::scope_type::type,
        typename shared_registration::storage_type::type,
        rebind_type_t<typename shared_registration::storage_type::type,
                      normalized_type_t<
                          typename shared_registration::storage_type::type>>,
        typename shared_registration::factory_type::type,
        typename shared_registration::conversions_type::type>;
    using shared_resolver =
        instance_resolver<rtti<typeid_provider>, A, shared_storage>;

    using shared_ptr_registration =
        type_registration<scope<shared>, storage<std::shared_ptr<A>>>;
    using shared_ptr_storage = detail::storage<
        typename shared_ptr_registration::scope_type::type,
        typename shared_ptr_registration::storage_type::type,
        rebind_type_t<typename shared_ptr_registration::storage_type::type,
                      normalized_type_t<typename shared_ptr_registration::
                                            storage_type::type>>,
        typename shared_ptr_registration::factory_type::type,
        typename shared_ptr_registration::conversions_type::type>;
    using shared_ptr_resolver =
        instance_resolver<rtti<typeid_provider>, A, shared_ptr_storage>;

    using external_registration =
        type_registration<scope<external>, storage<A&>>;
    using external_storage = detail::storage<
        typename external_registration::scope_type::type,
        typename external_registration::storage_type::type,
        rebind_type_t<typename external_registration::storage_type::type,
                      normalized_type_t<typename external_registration::
                                            storage_type::type>>,
        typename external_registration::factory_type::type,
        typename external_registration::conversions_type::type>;
    using external_resolver =
        instance_resolver<rtti<typeid_provider>, A, external_storage>;

    using external_shared_registration =
        type_registration<scope<external>, storage<std::shared_ptr<A>>>;
    using external_shared_storage = detail::storage<
        typename external_shared_registration::scope_type::type,
        typename external_shared_registration::storage_type::type,
        rebind_type_t<typename external_shared_registration::storage_type::type,
                      normalized_type_t<typename external_shared_registration::
                                            storage_type::type>>,
        typename external_shared_registration::factory_type::type,
        typename external_shared_registration::conversions_type::type>;
    using external_shared_resolver = instance_resolver<
        rtti<typeid_provider>, A, external_shared_storage>;

    using nested_registration =
        type_registration<scope<shared>,
                          storage<std::shared_ptr<std::unique_ptr<A>>>,
                          interfaces<I>>;
    using nested_storage_type = typename nested_registration::storage_type::type;
    using nested_interface_type =
        type_list_head_t<typename nested_registration::interface_type::type>;
    using nested_stored_type = rebind_leaf_t<
        nested_storage_type,
        std::conditional_t<
            std::has_virtual_destructor_v<nested_interface_type> &&
                is_interface_storage_rebindable_v<nested_storage_type,
                                                  nested_interface_type>,
            nested_interface_type, leaf_type_t<nested_storage_type>>>;

    // Nested wrapper requests can still borrow to the leaf interface, but
    // built-in smart handles only rebind when C++ itself has the direct handle
    // conversion.
    static_assert(is_interface_storage_rebindable_v<std::shared_ptr<A>, I>);
    static_assert(is_interface_storage_rebindable_v<std::unique_ptr<A>, I>);
    static_assert(
        !is_interface_storage_rebindable_v<std::shared_ptr<std::unique_ptr<A>>,
                                           I>);
    static_assert(
        !is_interface_storage_rebindable_v<std::unique_ptr<std::shared_ptr<A>>,
                                           I>);

    static_assert(
        std::is_same_v<nested_stored_type, std::shared_ptr<std::unique_ptr<A>>>);

    static_assert(std::is_empty_v<conversion_cache<type_list<>>>);
    static_assert(
        std::is_trivially_destructible_v<conversion_cache<type_list<>>>);
    static_assert(sizeof(shared_resolver) <= sizeof(shared_ptr_resolver));
    static_assert(sizeof(external_resolver) <= sizeof(external_shared_resolver));
}

TEST(type_registration_test, recursive_leaf_and_rebind_traits) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};

    using nested_handle = const std::shared_ptr<std::unique_ptr<A>>&;
    using nested_list = type_list<nested_handle, std::unique_ptr<A*>>;

    static_assert(std::is_same_v<leaf_type_t<nested_handle>, A>);
    static_assert(std::is_same_v<rebind_leaf_t<nested_handle, I>,
                                 std::shared_ptr<std::unique_ptr<I>>&>);
    static_assert(std::is_same_v<rebind_type_t<nested_handle, I>,
                                 std::shared_ptr<I>&>);
    static_assert(std::is_same_v<leaf_type_t<nested_list>,
                                 type_list<A, A>>);
    static_assert(std::is_same_v<rebind_leaf_t<nested_list, I>,
                                 type_list<std::shared_ptr<std::unique_ptr<I>>&,
                                           std::unique_ptr<I*>>>);

}

TEST(type_registration_test, normalized_type_trait) {
    struct I {
        virtual ~I() = default;
    };
    struct A : I {};
    struct annotation_tag {};

    static_assert(std::is_same_v<normalized_type_t<const std::shared_ptr<
                                     std::unique_ptr<A>>&>,
                                 A>);
    static_assert(
        std::is_same_v<normalized_type_t<annotated<I&, annotation_tag>>,
                       annotated<I, annotation_tag>>);
}
