//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/rtti/typeid_provider.h>
#include <dingo/type/type_map.h>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace dingo {
namespace {

struct dynamic_type_map_key_a {};
struct dynamic_type_map_key_b {};
struct dynamic_type_map_key_c {};

TEST(type_map_test, dynamic_erase_removes_entry_and_allows_reinsert) {
  std::allocator<char> allocator;
  dynamic_type_map<int, rtti<typeid_provider>, std::allocator<char>> values(
      allocator);

  ASSERT_TRUE(values.insert<dynamic_type_map_key_a>(1).second);
  ASSERT_TRUE(values.insert<dynamic_type_map_key_b>(2).second);
  ASSERT_TRUE(values.insert<dynamic_type_map_key_c>(3).second);
  ASSERT_EQ(values.size(), 3u);

  ASSERT_TRUE(values.erase<dynamic_type_map_key_b>());
  ASSERT_EQ(values.size(), 2u);
  ASSERT_EQ(values.get<dynamic_type_map_key_b>(), nullptr);

  std::vector<int> visited;
  for (auto entry : values) {
    visited.push_back(entry.second);
  }
  ASSERT_EQ(visited, (std::vector<int>{1, 3}));

  ASSERT_TRUE(values.erase<dynamic_type_map_key_c>());
  ASSERT_EQ(values.size(), 1u);
  ASSERT_EQ(values.front(), 1);

  auto inserted = values.insert<dynamic_type_map_key_b>(4);
  ASSERT_TRUE(inserted.second);
  ASSERT_EQ(*values.get<dynamic_type_map_key_b>(), 4);
  ASSERT_EQ(values.size(), 2u);

  ASSERT_FALSE(values.erase<dynamic_type_map_key_c>());
}

} // namespace
} // namespace dingo
