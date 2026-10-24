//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/base/mechanics.h>

#include <gtest/gtest.h>

#include <type_traits>

namespace {

using dingo::core::pack_at_portable_t;
using dingo::core::pack_at_t;
using dingo::core::type_list;
using dingo::core::type_list_at_t;

template <template <std::size_t, typename...> typename PackAt>
constexpr bool selects_every_element() {
  return std::is_same_v<PackAt<0, int, char, double>, int> &&
         std::is_same_v<PackAt<1, int, char, double>, char> &&
         std::is_same_v<PackAt<2, int, char, double>, double> &&
         std::is_same_v<PackAt<0, const int &>, const int &>;
}

static_assert(selects_every_element<pack_at_portable_t>());
static_assert(selects_every_element<pack_at_t>());
#if DINGO_HAS_TYPE_PACK_ELEMENT
static_assert(selects_every_element<dingo::core::pack_at_builtin_t>());
#endif
#if DINGO_HAS_PACK_INDEXING
static_assert(selects_every_element<dingo::core::pack_at_indexing_t>());
#endif

static_assert(type_list<int, char>::size == 2);
static_assert(std::is_same_v<type_list_at_t<1, type_list<int, char>>, char>);

TEST(mechanics, selects_the_variant_of_the_capabilities) {
#if DINGO_HAS_PACK_INDEXING
  EXPECT_EQ(DINGO_VARIANT_PACK_AT_INDEXING, 1);
#else
  EXPECT_EQ(DINGO_VARIANT_PACK_AT_INDEXING, 0);
#endif
  EXPECT_EQ(DINGO_VARIANT_PACK_AT_BUILTIN, DINGO_HAS_TYPE_PACK_ELEMENT);
}

} // namespace
