//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/resolution/type_cache.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/type/type_map.h>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace {
struct type_map_key_a {};
struct type_map_key_b {};
struct type_map_key_c {};

struct static_type_map_key_a {};
struct static_type_map_key_b {};
struct static_type_map_key_c {};
struct static_type_map_tag {};

struct throwing_move_only {
    explicit throwing_move_only(int value_in) : value(value_in) {}

    throwing_move_only(const throwing_move_only&) = delete;
    throwing_move_only& operator=(const throwing_move_only&) = delete;
    throwing_move_only(throwing_move_only&& other) noexcept(false)
        : value(other.value) {
        if (throw_on_move) {
            throw 1;
        }
        other.value = -1;
    }

    throwing_move_only& operator=(throwing_move_only&&) = delete;

    static bool throw_on_move;
    int value;
};

bool throwing_move_only::throw_on_move = false;
} // namespace

namespace dingo {
TEST(type_map_test, static_erase_unlinks_node_and_allows_reinsert) {
    std::allocator<char> allocator;
    static_type_map<int, static_type_map_tag, std::allocator<char>> values(
        allocator);

    ASSERT_TRUE(values.insert<static_type_map_key_a>(1).second);
    ASSERT_TRUE(values.insert<static_type_map_key_b>(2).second);
    ASSERT_TRUE(values.insert<static_type_map_key_c>(3).second);
    ASSERT_EQ(values.size(), 3u);

    ASSERT_TRUE(values.erase<static_type_map_key_b>());
    ASSERT_EQ(values.size(), 2u);
    ASSERT_EQ(values.get<static_type_map_key_b>(), nullptr);

    std::vector<int> visited;
    for (auto entry : values) {
        visited.push_back(entry.second);
    }
    ASSERT_EQ(visited, (std::vector<int>{3, 1}));

    ASSERT_TRUE(values.erase<static_type_map_key_c>());
    ASSERT_EQ(values.size(), 1u);
    ASSERT_EQ(values.front(), 1);

    auto inserted = values.insert<static_type_map_key_b>(4);
    ASSERT_TRUE(inserted.second);
    ASSERT_EQ(*values.get<static_type_map_key_b>(), 4);
    ASSERT_EQ(values.size(), 2u);

    ASSERT_FALSE(values.erase<static_type_map_key_c>());
}

TEST(type_map, sorted_vector_type_map_insert_get_and_erase) {
    using rtti_type = rtti<typeid_provider>;

    std::allocator<char> allocator;
    sorted_vector_type_map<int, rtti_type, std::allocator<char>> map(allocator);

    auto inserted_a = map.template insert<type_map_key_a>(11);
    auto inserted_c = map.template insert<type_map_key_c>(33);
    auto inserted_b = map.template insert<type_map_key_b>(22);
    auto duplicate_b = map.template insert<type_map_key_b>(44);

    EXPECT_TRUE(inserted_a.second);
    EXPECT_TRUE(inserted_b.second);
    EXPECT_TRUE(inserted_c.second);
    EXPECT_FALSE(duplicate_b.second);
    EXPECT_EQ(11, *map.template get<type_map_key_a>());
    EXPECT_EQ(22, *map.template get<type_map_key_b>());
    EXPECT_EQ(33, *map.template get<type_map_key_c>());
    EXPECT_EQ(3u, map.size());

    EXPECT_TRUE(map.template erase<type_map_key_b>());
    EXPECT_EQ(nullptr, map.template get<type_map_key_b>());
    EXPECT_EQ(2u, map.size());
    EXPECT_FALSE(map.template erase<type_map_key_b>());
}

TEST(type_map, sorted_vector_type_cache_insert_and_get) {
    using rtti_type = rtti<typeid_provider>;

    std::allocator<char> allocator;
    sorted_vector_type_cache<void*, rtti_type, std::allocator<char>> cache(
        allocator);
    int value = 7;

    EXPECT_EQ(nullptr, cache.template get<type_map_key_a>());

    cache.template insert<type_map_key_a>(&value);

    EXPECT_EQ(&value, cache.template get<type_map_key_a>());
    EXPECT_EQ(nullptr, cache.template get<type_map_key_b>());
}

TEST(type_map, sorted_vector_type_map_handles_empty_and_move_only_values) {
    using rtti_type = rtti<typeid_provider>;

    std::allocator<char> allocator;
    sorted_vector_type_map<std::unique_ptr<int>, rtti_type, std::allocator<char>>
        map(allocator);

    EXPECT_EQ(nullptr, map.template get<type_map_key_a>());
    EXPECT_FALSE(map.template erase<type_map_key_a>());

    auto inserted_a =
        map.template insert<type_map_key_a>(std::make_unique<int>(7));
    auto inserted_b =
        map.template insert<type_map_key_b>(std::make_unique<int>(11));

    ASSERT_TRUE(inserted_a.second);
    ASSERT_TRUE(inserted_b.second);
    ASSERT_NE(nullptr, map.template get<type_map_key_a>());
    ASSERT_NE(nullptr, map.template get<type_map_key_b>());
    EXPECT_EQ(7, **map.template get<type_map_key_a>());
    EXPECT_EQ(11, **map.template get<type_map_key_b>());

    EXPECT_TRUE(map.template erase<type_map_key_a>());
    EXPECT_EQ(nullptr, map.template get<type_map_key_a>());
    ASSERT_NE(nullptr, map.template get<type_map_key_b>());
    EXPECT_EQ(11, **map.template get<type_map_key_b>());
}

TEST(type_map, sorted_vector_type_map_supports_throwing_move_only_values) {
    using rtti_type = rtti<typeid_provider>;

    std::allocator<char> allocator;
    sorted_vector_type_map<throwing_move_only, rtti_type, std::allocator<char>>
        map(allocator);

    ASSERT_TRUE(
        map.template insert<type_map_key_a>(throwing_move_only{7}).second);

    throwing_move_only::throw_on_move = true;
    EXPECT_THROW(map.template insert<type_map_key_b>(throwing_move_only{11}),
                 int);
    throwing_move_only::throw_on_move = false;

    ASSERT_NE(nullptr, map.template get<type_map_key_a>());
    EXPECT_EQ(7, map.template get<type_map_key_a>()->value);
    ASSERT_TRUE(
        map.template insert<type_map_key_b>(throwing_move_only{11}).second);
    ASSERT_NE(nullptr, map.template get<type_map_key_b>());
    EXPECT_EQ(11, map.template get<type_map_key_b>()->value);
}
} // namespace dingo
