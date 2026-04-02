//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>

#include <gtest/gtest.h>

#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

namespace dingo {
template <typename T> struct shared_cyclical_regression_test : public test<T> {};
TYPED_TEST_SUITE(shared_cyclical_regression_test, container_types, );

TYPED_TEST(shared_cyclical_regression_test, shared_ptr_conversion_rollback) {
    using container_type = TypeParam;

    struct Ex {};
    struct B;
    struct A : Class {
        explicit A(std::shared_ptr<IClass1>& ibptr) : ibptr_(ibptr) {}
        std::shared_ptr<IClass1>& ibptr_;
    };

    struct B : Class {
        explicit B(A&) {
            if (fail_flag()) {
                fail_flag() = false;
                throw Ex();
            }
        }

        static bool& fail_flag() {
            static bool fail = true;
            return fail;
        }
    };
    B::fail_flag() = true;

    container_type container;
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<A>>>();
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<B>>,
                                     interfaces<B, IClass1>>();

    ASSERT_THROW(container.template resolve<A&>(), Ex);

    auto& a = container.template resolve<A&>();
    auto& b = container.template resolve<B&>();
    ASSERT_EQ(static_cast<IClass1*>(&b), a.ibptr_.get());
}

TYPED_TEST(
    shared_cyclical_regression_test,
    shared_ptr_conversion_commit_survives_later_failure) {
    using container_type = TypeParam;

    struct Ex {};
    struct B;
    struct A : Class {
        explicit A(std::shared_ptr<IClass1>& ibptr) : ibptr_(ibptr) {}
        std::shared_ptr<IClass1>& ibptr_;
    };

    struct B : Class {
        explicit B(A&) {}
    };

    struct C : Class {
        explicit C(std::shared_ptr<IClass1>&) { throw Ex(); }
    };

    container_type container;
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<A>>>();
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<B>>,
                                     interfaces<B, IClass1>>();
    container.template register_type<scope<shared>, storage<C>>();

    auto& a = container.template resolve<A&>();
    auto& b = container.template resolve<B&>();
    ASSERT_EQ(static_cast<IClass1*>(&b), a.ibptr_.get());

    ASSERT_THROW(container.template resolve<C&>(), Ex);
    ASSERT_EQ(static_cast<IClass1*>(&b), a.ibptr_.get());
}
} // namespace dingo
