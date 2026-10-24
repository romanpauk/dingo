//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The list of types of the next implementation: its size, the element at an
// index, and the position of the first element of a type.

#include <dingo/base/mechanics.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <type_traits>
#include <vector>

namespace {

using dingo::core::type_list;
using dingo::core::type_list_at_t;
using dingo::core::type_list_index_of;

struct a {};
struct b {};
struct c {};

using nested = type_list<a, type_list<b, c>>;

static_assert(type_list<>::size == 0);
static_assert(nested::size == 2);
static_assert(type_list<a, b, a, c, b>::size == 5);

static_assert(std::is_same_v<type_list_at_t<0, nested>, a>);
static_assert(std::is_same_v<type_list_at_t<1, nested>, type_list<b, c>>);
static_assert(std::is_same_v<type_list_at_t<2, type_list<a, b, c>>, c>);

// The position of the first element of a type, or the size of a list without
// one; a nested list is an element, not its elements.
static_assert(type_list_index_of<a, type_list<a, b, a>>() == 0);
static_assert(type_list_index_of<b, type_list<a, b, a>>() == 1);
static_assert(type_list_index_of<c, type_list<a, b>>() == 2);
static_assert(type_list_index_of<a, type_list<>>() == 0);
static_assert(type_list_index_of<c, nested>() == nested::size);
static_assert(type_list_index_of<type_list<b, c>, nested>() == 1);
static_assert(type_list_index_of<const a, type_list<a, const a>>() == 1);

template <typename... Types>
std::vector<std::size_t> positions(type_list<Types...> /*list*/) {
  return {type_list_index_of<Types, type_list<Types...>>()...};
}

TEST(next_type_list, finds_the_first_element_of_each_type) {
  EXPECT_EQ(positions(type_list<a, b, c>{}),
            (std::vector<std::size_t>{0, 1, 2}));
  EXPECT_EQ(positions(type_list<a, b, a, c, b>{}),
            (std::vector<std::size_t>{0, 1, 0, 3, 1}));
  EXPECT_TRUE(positions(type_list<>{}).empty());
}

} // namespace
