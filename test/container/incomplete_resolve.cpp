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

namespace dingo {
namespace incomplete_resolve_test {

struct service;

void register_service(container<>& container);

TEST(container_test, resolve_incomplete_pointer_and_reference) {
    container<> container;
    register_service(container);

    auto* ptr = container.template resolve<service*>();
    auto& ref = container.template resolve<service&>();

    EXPECT_EQ(ptr, std::addressof(ref));
}

TEST(container_test,
     construct_registered_type_with_incomplete_reference_dependency) {
    container<> container;
    register_service(container);
    container.template register_type<scope<unique>, storage<consumer>>();

    auto built = container.template resolve<consumer>();

    EXPECT_EQ(built.value, container.template resolve<service*>());
}

} // namespace incomplete_resolve_test
} // namespace dingo
