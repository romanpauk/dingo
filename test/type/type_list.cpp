//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/base/type_list.h>
#include <dingo/rtti/typeid_provider.h>

#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

namespace dingo {
namespace {

struct type_list_a {};
struct type_list_b {};
struct type_list_c {};

TEST(type_list_test, meta_utilities) {
  using nested_list =
      type_list<type_list_a, type_list<type_list_b, type_list_c>>;

  static_assert(
      std::is_same_v<
          type_list_cat_t<type_list<type_list_a>,
                          type_list<type_list_b, type_list_c>, type_list<>>,
          type_list<type_list_a, type_list_b, type_list_c>>);
  static_assert(type_list_size_v<type_list<>> == 0);
  static_assert(type_list_size_v<nested_list> == 2);
  static_assert(std::is_same_v<type_list_head_t<nested_list>, type_list_a>);
  static_assert(!type_list_contains_v<type_list_a, type_list<>>);
  static_assert(type_list_contains_v<type_list_a, nested_list>);
  static_assert(!type_list_contains_v<type_list_c, nested_list>);
  static_assert(std::is_same_v<type_list_unique_t<type_list<>>, type_list<>>);
  static_assert(
      std::is_same_v<
          type_list_unique_t<type_list<type_list_a, type_list_b, type_list_a,
                                       type_list_c, type_list_b>>,
          type_list<type_list_a, type_list_b, type_list_c>>);
  static_assert(
      std::is_same_v<type_list_merge_t<type_list<type_list_a, type_list_b>,
                                       type_list<type_list_b, type_list_c>>,
                     type_list<type_list_a, type_list_b, type_list_c>>);
  static_assert(
      std::is_same_v<
          type_list_merge_t<type_list<>, type_list<type_list_a, type_list_a>>,
          type_list<type_list_a, type_list_a>>);
  static_assert(
      std::is_same_v<
          type_list_merge_t<type_list<type_list_a, type_list_a>, type_list<>>,
          type_list<type_list_a, type_list_a>>);
}

TEST(type_list_test, indexed_access) {
  using list = type_list<type_list_a, type_list_b, type_list_a>;

  static_assert(std::is_same_v<type_list_element_t<0, list>, type_list_a>);
  static_assert(std::is_same_v<type_list_element_t<1, list>, type_list_b>);
  static_assert(std::is_same_v<type_list_element_t<2, list>, type_list_a>);
  static_assert(
      std::is_same_v<detail::pack_element_portable_t<1, type_list_a,
                                                     type_list_b, type_list_a>,
                     type_list_b>);
  static_assert(
      std::is_same_v<detail::pack_element_portable_t<2, type_list_a,
                                                     type_list_b, type_list_a>,
                     type_list_a>);

  static_assert(pack_find<>() == 0);
  static_assert(pack_find<false, true, true>() == 1);
  static_assert(pack_find<false, false>() == 2);

  static_assert(std::is_same_v<type_list_find_t<void, type_list<>>, void>);
  static_assert(std::is_same_v<type_list_find_t<void, list, false, true, true>,
                               type_list_b>);
  static_assert(
      std::is_same_v<type_list_find_t<type_list_c, list, false, false, false>,
                     type_list_c>);
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

} // namespace
} // namespace dingo
