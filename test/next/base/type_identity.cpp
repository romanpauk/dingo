//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The identity of a type is one in every translation unit, however a unit
// spells the type. Names std::unique_ptr<service> first; linked with
// type_identity_fixture.cpp, which names the same type with its default
// deleter spelled out first. spec: variant_points.named_type_id

#include "base/type_identity_shared.h"

#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>

namespace next_type_identity {

using dingo::type_id;
using dingo::type_id_v;
using dingo::type_tree_view_v;

using bare_spelling = std::unique_ptr<service>;

type_id bare_spelling_id() {
  constexpr type_id id = type_id_v<bare_spelling>;
  return id;
}

constexpr bool same(type_id left, type_id right) { return left == right; }

// Both spellings are one type, whose identity compares equal when constant
// evaluated, as a lookup table compares it.
static_assert(
    same(type_id_v<bare_spelling>,
         type_id_v<std::unique_ptr<service, std::default_delete<service>>>));
static_assert(!same(type_id_v<bare_spelling>, type_id_v<service>));
static_assert(same(type_tree_view_v<bare_spelling>.nodes[1].type,
                   type_id_v<service>));

TEST(next_type_identity, the_identity_of_a_type_is_one_across_units) {
  EXPECT_TRUE(bare_spelling_id() == defaulted_spelling_id());
  EXPECT_TRUE(defaulted_spelling_id() == bare_spelling_id());
  EXPECT_TRUE(same(bare_spelling_id(), type_id_v<bare_spelling>));
  EXPECT_TRUE(defaulted_spelling_leaf_id() == type_id_v<service>);
}

TEST(next_type_identity, the_identities_of_different_types_differ) {
  EXPECT_FALSE(bare_spelling_id() == type_id_v<service>);
  EXPECT_FALSE(bare_spelling_id() == type_id_v<std::shared_ptr<service>>);
  EXPECT_FALSE(bare_spelling_id() == type_id_v<service *>);
  EXPECT_FALSE(bare_spelling_id() == type_id_v<const bare_spelling>);
}

TEST(next_type_identity, resolves_across_units_by_either_spelling) {
  dingo::container<> container;
  register_defaulted_spelling(container);

  EXPECT_EQ(resolve_defaulted_spelling(container), 7);
  EXPECT_EQ(container.resolve<bare_spelling>()->value, 7);
}

} // namespace next_type_identity
