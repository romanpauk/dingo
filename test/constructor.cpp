//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/class_traits.h>

#include <gtest/gtest.h>

namespace dingo {
TEST(constructor, basic) {
    struct A {};
    struct B {
        B(std::shared_ptr<A>) {}
    };

    class_traits<A>::construct();
    class_traits<B>::construct(nullptr);
    delete class_traits<B*>::construct(nullptr);
    class_traits<std::unique_ptr<B>>::construct(nullptr);
    class_traits<std::shared_ptr<B>>::construct(nullptr);
    class_traits<std::optional<B>>::construct(nullptr);
}
} // namespace dingo