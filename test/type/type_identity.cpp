//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Names std::unique_ptr<service> first. Linked with type_identity_fixture.cpp,
// which names the same type with its default deleter spelled out first.

#include "type_identity_shared.h"

#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>

namespace dingo {
namespace type_identity_test {

using bare_spelling = std::unique_ptr<service>;

type_descriptor bare_spelling_descriptor() {
  constexpr auto descriptor = describe_type<bare_spelling>();
  return descriptor;
}

TEST(type_identity_test, descriptors_of_one_type_compare_equal_across_units) {
  EXPECT_TRUE(bare_spelling_descriptor() == defaulted_spelling_descriptor());
  EXPECT_TRUE(defaulted_spelling_descriptor() == bare_spelling_descriptor());
  EXPECT_TRUE(bare_spelling_descriptor() == describe_type<bare_spelling>());
}

TEST(type_identity_test, descriptors_of_different_types_compare_unequal) {
  EXPECT_FALSE(bare_spelling_descriptor() == describe_type<service>());
  EXPECT_FALSE(bare_spelling_descriptor() ==
               describe_type<std::shared_ptr<service>>());
  EXPECT_FALSE(bare_spelling_descriptor() == describe_type<service *>());
}

TEST(type_identity_test, resolve_across_units_with_either_spelling) {
  container<> container;
  register_defaulted_spelling(container);

  EXPECT_EQ(resolve_defaulted_spelling(container), 7);
  EXPECT_EQ(container.template resolve<bare_spelling>()->value, 7);
}

} // namespace type_identity_test
} // namespace dingo
