//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Force the portable pack selection so the fallback is compiled even on a
// compiler that provides __type_pack_element.
#define DINGO_VARIANT_PACK_AT_INDEXING 0
#define DINGO_VARIANT_PACK_AT_BUILTIN 0

#include <dingo/base/type_list.h>

#include <gtest/gtest.h>

#include <type_traits>

namespace dingo {
namespace {

struct portable_a;
struct portable_b;
struct portable_c;

static_assert(
    std::is_same_v<type_list_at_t<type_list<portable_a, portable_b>, 0>,
                   portable_a>);
static_assert(
    std::is_same_v<type_list_at_t<type_list<portable_a, portable_b>, 1>,
                   portable_b>);
static_assert(std::is_same_v<
              type_list_at_t<type_list<portable_a, portable_b, portable_c>, 2>,
              portable_c>);

TEST(type_list_portable_test, pack_selection) { SUCCEED(); }

} // namespace
} // namespace dingo
