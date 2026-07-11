//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/memory/tagged_ptr.h>

#include <gtest/gtest.h>

namespace dingo {
namespace {

struct alignas(8) tagged_value {
  int value;
};

static_assert(sizeof(tagged_ptr<tagged_value>) == sizeof(tagged_value *));

TEST(tagged_ptr_test, stores_pointer_and_tag_in_one_word) {
  tagged_value value{7};
  tagged_ptr<tagged_value, 2> pointer(&value, 3);

  EXPECT_EQ(pointer.get(), &value);
  EXPECT_EQ(pointer.tag(), 3);
  EXPECT_EQ(pointer->value, 7);
  EXPECT_EQ((*pointer).value, 7);
  EXPECT_TRUE(pointer);
}

TEST(tagged_ptr_test, updates_pointer_without_changing_tag) {
  tagged_value first{1};
  tagged_value second{2};
  tagged_ptr<tagged_value, 2> pointer(&first, 2);

  pointer.set_pointer(&second);

  EXPECT_EQ(pointer.get(), &second);
  EXPECT_EQ(pointer.tag(), 2);
}

TEST(tagged_ptr_test, updates_tag_without_changing_pointer) {
  tagged_value value{7};
  tagged_ptr<tagged_value, 2> pointer(&value);

  pointer.set_tag(3);

  EXPECT_EQ(pointer.get(), &value);
  EXPECT_EQ(pointer.tag(), 3);
}

TEST(tagged_ptr_test, supports_a_tagged_null_pointer) {
  tagged_ptr<tagged_value, 2> pointer(nullptr, 2);

  EXPECT_EQ(pointer.get(), nullptr);
  EXPECT_EQ(pointer.tag(), 2);
  EXPECT_FALSE(pointer);
}

} // namespace
} // namespace dingo
