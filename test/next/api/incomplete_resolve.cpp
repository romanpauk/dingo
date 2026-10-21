//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A request for a pointer or a reference to a type that is incomplete where
// it is resolved, registered where the type is complete.

#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "incomplete_resolve_shared.h"

#include <memory>

namespace next_incomplete_resolve {

TEST(next_incomplete_resolve,
     resolves_a_pointer_and_a_reference_to_an_incomplete_type) {
  dingo::container<> container;
  register_service(container);

  auto *ptr = container.resolve<service *>();
  auto &ref = container.resolve<service &>();

  EXPECT_EQ(ptr, std::addressof(ref));
}

TEST(next_incomplete_resolve, injects_a_reference_to_an_incomplete_type) {
  dingo::container<> container;
  register_service(container);
  container
      .register_type<dingo::scope<dingo::unique>, dingo::storage<consumer>>();

  auto built = container.resolve<consumer>();

  EXPECT_EQ(built.value, container.resolve<service *>());
}

} // namespace next_incomplete_resolve
