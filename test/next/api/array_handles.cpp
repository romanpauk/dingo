//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// spec: routes
// An array leaf is served as itself, its rows, and its elements; no route
// adopts it into an owning handle of an array of runtime size.

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <memory>

namespace {

struct cell {};

TEST(next_array_handles, a_unique_array_is_not_adopted_into_an_owning_handle) {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::unique>, dingo::storage<cell[4]>>();
  EXPECT_THROW(container.resolve<std::unique_ptr<cell[]>>(),
               dingo::type_not_convertible_exception);
}

TEST(next_array_handles, a_shared_array_is_not_adopted_into_an_owning_handle) {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<cell[4]>>();
  EXPECT_THROW(container.resolve<std::unique_ptr<cell[]>>(),
               dingo::type_not_convertible_exception);
  EXPECT_NE(container.resolve<cell(*)[4]>(), nullptr);
}

} // namespace
