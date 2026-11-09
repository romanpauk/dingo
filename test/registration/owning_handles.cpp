//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A scope makes the owning handles of the families owning_handles lists, a
// user's as the standard ones. The list is one for a program, so this test is
// a program of its own.

#include "support/custom_wrappers.h"

#include <dingo/type/type_traits.h>

template <> struct dingo::owning_handles<> {
  using type = dingo::type_list<std::unique_ptr<dingo::handle_element>,
                                std::shared_ptr<dingo::handle_element>,
                                dingo::test_unique<dingo::handle_element>,
                                dingo::test_shared<dingo::handle_element>>;
};

#include <dingo/container.h>
#include <dingo/static/binding.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>

namespace {
struct interface_type {
  virtual ~interface_type() = default;
  virtual int value() const = 0;
};

struct leaf : interface_type {
  int value() const override { return 7; }
};
} // namespace

namespace dingo {
TEST(owning_handles_test, unique_value_is_moved_into_a_listed_handle) {
  container<> runtime;
  runtime.register_type<scope<unique>, storage<leaf>,
                        interfaces<leaf, interface_type>>();
  EXPECT_EQ(runtime.resolve<test_unique<leaf>>().get()->value(), 7);
  EXPECT_EQ(runtime.resolve<test_shared<leaf>>().get()->value(), 7);
  EXPECT_EQ(runtime.resolve<test_unique<interface_type>>().get()->value(), 7);
  EXPECT_EQ(runtime.resolve<test_shared<interface_type>>().get()->value(), 7);
  EXPECT_EQ(runtime.resolve<std::unique_ptr<interface_type>>()->value(), 7);

  static_container<
      bindings<bind<scope<unique>, storage<leaf>, interfaces<interface_type>>>>
      compile_time;
  EXPECT_EQ(compile_time.resolve<test_unique<interface_type>>().get()->value(),
            7);
  EXPECT_EQ(compile_time.resolve<test_shared<interface_type>>().get()->value(),
            7);
}

TEST(owning_handles_test, unique_pointer_is_taken_over_by_a_listed_handle) {
  container<> runtime;
  runtime.register_type<scope<unique>, storage<leaf *>>();
  EXPECT_EQ(runtime.resolve<test_unique<leaf>>().get()->value(), 7);
  EXPECT_EQ(runtime.resolve<test_shared<leaf>>().get()->value(), 7);

  static_container<bindings<bind<scope<unique>, storage<leaf *>>>> compile_time;
  EXPECT_EQ(compile_time.resolve<test_unique<leaf>>().get()->value(), 7);
}
} // namespace dingo
