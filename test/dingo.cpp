//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type_list.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct dingo_test : public test<T> {};
TYPED_TEST_SUITE(dingo_test, container_types, );

TYPED_TEST(dingo_test, unique_hierarchy) {
    using container_type = TypeParam;

    struct S {};
    struct U {};
    struct B {
        B(std::shared_ptr<S>&&) {}
    };

    container_type container;
    container
        .template register_type<scope<unique>, storage<std::shared_ptr<S>>>();
    container
        .template register_type<scope<unique>, storage<std::unique_ptr<U>>>();
    container.template register_type<scope<unique>, storage<B>>();

    container.template resolve<B>();
}

TYPED_TEST(dingo_test, resolve_rollback) {
    using container_type = TypeParam;

    struct A : ClassTag<0> {};
    struct B : ClassTag<1> {};
    struct Ex {};
    struct C {
        C(A&, B&) { throw Ex(); }
    };

    container_type container;
    container.template register_type<scope<shared>, storage<A>>();
    container.template register_type<scope<shared>, storage<B>>();
    container.template register_type<scope<shared>, storage<C>>();

    ASSERT_THROW(container.template resolve<C&>(), Ex);
    ASSERT_EQ(A::Constructor, 1);
    ASSERT_EQ(A::Destructor, 0);
    ASSERT_EQ(B::Constructor, 1);
    ASSERT_EQ(B::Destructor, 0);

    container.template resolve<A&>();
    ASSERT_EQ(A::Constructor, 1);
    ASSERT_EQ(A::Destructor, 0);
    ASSERT_THROW(container.template resolve<C>(), Ex);
    ASSERT_EQ(A::Constructor, 1);
    ASSERT_EQ(A::Destructor, 0);
    ASSERT_EQ(B::Constructor, 1);
    ASSERT_EQ(B::Destructor, 0);
}

TYPED_TEST(dingo_test, type_already_registered) {
    using container_type = TypeParam;

    container_type container;
    {
        container.template register_type<scope<shared>, storage<Class>>();
        auto reg = [&] {
            container.template register_type<scope<shared>, storage<Class>>();
        };
        ASSERT_THROW(reg(), dingo::type_already_registered_exception);
    }
    {
        container.template register_type<scope<shared>, storage<Class>,
                                         interfaces<IClass>>();
        auto reg = [&] {
            container.template register_type<scope<shared>, storage<Class>,
                                             interfaces<IClass>>();
            ;
        };
        ASSERT_THROW(reg(), dingo::type_already_registered_exception);
    }
}
} // namespace dingo
