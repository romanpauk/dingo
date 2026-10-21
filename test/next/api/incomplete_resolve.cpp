//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// A request for a pointer or a reference to a type that is incomplete where
// it is resolved, registered where the type is complete.
//
// Disabled: the next implementation does not compile these requests. The
// shape of a request describes its leaf with traits that need a complete
// type (dingo/core/projection/layer_traits.h, make_shape, copyable and
// constructible), and so does the condition of the factory step of the
// selection (dingo/core/projection/construct.h, has_detected_factory). Define
// DINGO_TEST_INCOMPLETE_REQUESTS to compile the tests.

#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "incomplete_resolve_shared.h"

#include <memory>

namespace next_incomplete_resolve {

TEST(next_incomplete_resolve,
     DISABLED_resolves_a_pointer_and_a_reference_to_an_incomplete_type) {
#if defined(DINGO_TEST_INCOMPLETE_REQUESTS)
  dingo::container<> container;
  register_service(container);

  auto *ptr = container.resolve<service *>();
  auto &ref = container.resolve<service &>();

  EXPECT_EQ(ptr, std::addressof(ref));
#endif
}

TEST(next_incomplete_resolve,
     DISABLED_injects_a_reference_to_an_incomplete_type) {
#if defined(DINGO_TEST_INCOMPLETE_REQUESTS)
  dingo::container<> container;
  register_service(container);
  container
      .register_type<dingo::scope<dingo::unique>, dingo::storage<consumer>>();

  auto built = container.resolve<consumer>();

  EXPECT_EQ(built.value, container.resolve<service *>());
#endif
}

} // namespace next_incomplete_resolve
