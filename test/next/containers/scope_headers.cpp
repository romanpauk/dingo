//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The scopes of the library are complete wherever the container is: a
// registration with a built-in scope needs no header of its own.

#include <dingo/container.h>

#include <gtest/gtest.h>

namespace {

struct service {
  service() : value(3) {}
  int value;
};

TEST(next_scope_headers, the_container_declares_the_scopes_of_the_library) {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>, dingo::storage<int>>();
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
  EXPECT_EQ(container.resolve<int>(), 0);
  EXPECT_EQ(container.resolve<service &>().value, 3);
}

} // namespace
