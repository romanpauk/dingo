//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <gtest/gtest.h>

#include "incomplete_resolve_shared.h"

#include <dingo/storage/unique.h>

#include <memory>
#include <optional>

namespace dingo {
namespace incomplete_resolve_test {

TEST(container_test, resolve_incomplete_pointer_and_reference) {
  container<> container;
  register_service(container);

  auto *ptr = container.template resolve<service *>();
  auto &ref = container.template resolve<service &>();

  EXPECT_EQ(ptr, std::addressof(ref));
}

TEST(container_test,
     construct_registered_type_with_incomplete_reference_dependency) {
  container<> container;
  register_service(container);
  container.template register_type<scope<unique>, storage<consumer>>();

  auto built = container.template resolve<consumer>();

  EXPECT_EQ(built.value, container.template resolve<service *>());
}

// A handle of a type only declared here is resolved as the scope stores it:
// by value, by reference or by pointer, as its element, and copied into an
// optional, which needs no complete element.
TEST(container_test, resolve_handle_of_incomplete_type) {
  container<> container;
  register_handle_service(container);

  auto &stored =
      container.template resolve<std::shared_ptr<handle_service> &>();
  ASSERT_NE(stored, nullptr);
  EXPECT_EQ(container.template resolve<std::shared_ptr<handle_service>>(),
            stored);
  EXPECT_EQ(container.template resolve<std::shared_ptr<handle_service> *>(),
            std::addressof(stored));
  EXPECT_EQ(std::addressof(container.template resolve<handle_service &>()),
            stored.get());
  EXPECT_EQ(value_of(container.template resolve<handle_service &>()), 9);
  EXPECT_EQ(container.invoke(
                [](std::shared_ptr<handle_service> handle) { return handle; }),
            stored);
  auto optional =
      container
          .template resolve<std::optional<std::shared_ptr<handle_service>>>();
  ASSERT_TRUE(optional.has_value());
  EXPECT_EQ(*optional, stored);
}

} // namespace incomplete_resolve_test
} // namespace dingo
