//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/type/type_map.h>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace dingo {
namespace {

struct static_type_map_key_a {};
struct static_type_map_key_b {};
struct static_type_map_key_c {};
struct static_type_map_tag {};

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

} // namespace
} // namespace dingo
