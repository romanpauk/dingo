#include <dingo/type_registration.h>

#include <gtest/gtest.h>

using namespace dingo;

TEST(dsl, test_get_type) {
    struct A {};
    using list = type_list<storage<A>, factory<A>, interface<A, A>>;
    using storage_type = detail::get_type_t<storage<void>, list>;
    static_assert(std::is_same_v<storage_type, storage<A>>);

    using factory_type = detail::get_type_t<factory<void>, list>;
    static_assert(std::is_same_v<factory_type, factory<A>>);

    using interface_type = detail::get_type_t<interface<void>, list>;
    static_assert(std::is_same_v<interface_type, interface<A, A>>);
}

TEST(dsl, test_type_registration_basic) {
    using registration = type_registration<storage<int>, interface<double, float>, factory<int>, scope<int>>;

    static_assert(std::is_same_v<typename registration::storage_type, storage<int>>);
    static_assert(std::is_same_v<typename registration::interface_type, interface<double, float>>);
    static_assert(std::is_same_v<typename registration::factory_type, factory<int>>);
    static_assert(std::is_same_v<typename registration::scope_type, scope<int>>);
}

TEST(dsl, test_type_registration_deduction) {
    static_assert(std::is_same_v<typename type_registration<scope<int>, factory<int>>::storage_type, storage<int>>);
    static_assert(std::is_same_v<typename type_registration<scope<int>, factory<int>>::interface_type, interface<int>>);

    static_assert(std::is_same_v<typename type_registration<scope<int>, storage<int>>::interface_type, interface<int>>);
    static_assert(
        std::is_same_v<typename type_registration<scope<int>, storage<int>>::factory_type, factory<constructor<int>>>);
}
