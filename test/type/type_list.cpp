//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/rtti/typeid_provider.h>
#include <dingo/type/type_list.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

namespace dingo {
namespace {

struct type_list_a {};
struct type_list_b {};
struct type_list_c {};

TEST(type_list_test, meta_utilities) {
    using nested_tuple =
        std::tuple<type_list_a, std::tuple<type_list_b, type_list_c>>;
    using nested_list =
        type_list<type_list_a, type_list<type_list_b, type_list_c>>;

    static_assert(std::is_same_v<
                  type_list_cat_t<type_list<type_list_a>,
                                  type_list<type_list_b, type_list_c>,
                                  type_list<>>,
                  type_list<type_list_a, type_list_b, type_list_c>>);
    static_assert(type_list_size_v<type_list<>> == 0);
    static_assert(type_list_size_v<nested_list> == 2);
    static_assert(std::is_same_v<type_list_head_t<nested_list>, type_list_a>);
    static_assert(std::is_same_v<to_type_list_t<nested_tuple>, nested_list>);
    static_assert(std::is_same_v<to_tuple_t<nested_list>, nested_tuple>);
    static_assert(std::is_same_v<to_tuple_t<to_type_list_t<nested_tuple>>,
                                 nested_tuple>);
}

TEST(type_list_test, for_each_visits_types_in_order) {
    std::vector<int> visited;

    for_each(type_list<type_list_a, type_list_b, type_list_c>{}, [&](auto type) {
        using current = typename decltype(type)::type;
        if constexpr (std::is_same_v<current, type_list_a>)
            visited.push_back(1);
        else if constexpr (std::is_same_v<current, type_list_b>)
            visited.push_back(2);
        else if constexpr (std::is_same_v<current, type_list_c>)
            visited.push_back(3);
    });

    ASSERT_EQ(visited, (std::vector<int>{1, 2, 3}));

    bool called = false;
    for_each(type_list<>{}, [&](auto) { called = true; });
    ASSERT_FALSE(called);
}

TEST(type_list_test, for_type_matches_first_type_only) {
    using test_rtti = rtti<typeid_provider>;

    int called = 0;
    const auto matched = for_type<test_rtti>(
        type_list<type_list_a, type_list_b, type_list_a>{},
        test_rtti::get_type_index<type_list_a>(), [&](auto type) {
            ++called;
            using current = typename decltype(type)::type;
            ASSERT_TRUE((std::is_same_v<current, type_list_a>));
        });

    ASSERT_TRUE(matched);
    ASSERT_EQ(called, 1);

    called = 0;
    const auto missing = for_type<test_rtti>(
        type_list<type_list_a, type_list_b>{},
        test_rtti::get_type_index<type_list_c>(),
        [&](auto) { ++called; });

    ASSERT_FALSE(missing);
    ASSERT_EQ(called, 0);

    const auto empty = for_type<test_rtti>(
        type_list<>{}, test_rtti::get_type_index<type_list_a>(),
        [&](auto) { ++called; });

    ASSERT_FALSE(empty);
    ASSERT_EQ(called, 0);
}

} // namespace
} // namespace dingo
