//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/class_instance_conversions.h>
#include <dingo/class_instance_resolver.h>
#include <dingo/rebind_type.h>
#include <dingo/type_identity/type_identity.h>
#include <dingo/type_identity/typeid_provider.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/type_registration.h>

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
    struct A {};

    using shared_registration = type_registration<scope<shared>, storage<A>>;
    using shared_storage = detail::storage<
        typename shared_registration::scope_type::type,
        typename shared_registration::storage_type::type,
        rebind_type_t<typename shared_registration::storage_type::type,
                      decay_t<typename shared_registration::storage_type::type>>,
        typename shared_registration::factory_type::type,
        typename shared_registration::conversions_type::type>;
    using shared_resolver =
        class_instance_resolver<type_identity<typeid_provider>, A, shared_storage>;

    using shared_ptr_registration =
        type_registration<scope<shared>, storage<std::shared_ptr<A>>>;
    using shared_ptr_storage = detail::storage<
        typename shared_ptr_registration::scope_type::type,
        typename shared_ptr_registration::storage_type::type,
        rebind_type_t<typename shared_ptr_registration::storage_type::type,
                      decay_t<typename shared_ptr_registration::storage_type::type>>,
        typename shared_ptr_registration::factory_type::type,
        typename shared_ptr_registration::conversions_type::type>;
    using shared_ptr_resolver =
        class_instance_resolver<type_identity<typeid_provider>, A, shared_ptr_storage>;

    using external_registration =
        type_registration<scope<external>, storage<A&>>;
    using external_storage = detail::storage<
        typename external_registration::scope_type::type,
        typename external_registration::storage_type::type,
        rebind_type_t<typename external_registration::storage_type::type,
                      decay_t<typename external_registration::storage_type::type>>,
        typename external_registration::factory_type::type,
        typename external_registration::conversions_type::type>;
    using external_resolver =
        class_instance_resolver<type_identity<typeid_provider>, A, external_storage>;

    using external_shared_registration =
        type_registration<scope<external>, storage<std::shared_ptr<A>>>;
    using external_shared_storage = detail::storage<
        typename external_shared_registration::scope_type::type,
        typename external_shared_registration::storage_type::type,
        rebind_type_t<typename external_shared_registration::storage_type::type,
                      decay_t<typename external_shared_registration::storage_type::type>>,
        typename external_shared_registration::factory_type::type,
        typename external_shared_registration::conversions_type::type>;
    using external_shared_resolver = class_instance_resolver<
        type_identity<typeid_provider>, A, external_shared_storage>;

    static_assert(std::is_empty_v<class_instance_conversions<type_list<>>>);
    static_assert(
        std::is_trivially_destructible_v<class_instance_conversions<type_list<>>>);
    static_assert(sizeof(shared_resolver) < sizeof(shared_ptr_resolver));
    static_assert(sizeof(external_resolver) < sizeof(external_shared_resolver));
}
