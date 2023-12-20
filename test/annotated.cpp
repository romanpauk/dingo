//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/annotated.h>
#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/type_list.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct annotated_test : public test<T> {};
TYPED_TEST_SUITE(annotated_test, container_types);

template <size_t N> struct tag {};

TYPED_TEST(annotated_test, value) {
    using container_type = TypeParam;

    struct A {
        A(annotated<int, tag<1>>, annotated<int, tag<2>>) {}
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>,
                                     interface<annotated<int, tag<1>>>>(1);
    container.template register_type<scope<unique>, storage<int>,
                                     interface<annotated<int, tag<2>>>>(
        callable([] { return 10; }));
    container.template register_type<scope<unique>, storage<A>>();

    container.template resolve<annotated<int, tag<1>>>();
    container.template resolve<annotated<int, tag<2>>>();
    container.template resolve<A>();
}

TYPED_TEST(annotated_test, interface) {
    using container_type = TypeParam;

    struct I {
        virtual ~I() {}
    };

    struct A : I {};
    struct B : I {};

    struct C : I {
        C(annotated<I&, tag<1>> ref, annotated<I*, tag<1>> ptr,
          annotated<std::shared_ptr<I>, tag<2>> sharedptr)
            : ref_(ref), ptr_(ptr), sharedptr_(sharedptr) {
            EXPECT_TRUE(dynamic_cast<A*>(&ref_));
            EXPECT_TRUE(dynamic_cast<A*>(ptr_));
            EXPECT_TRUE(dynamic_cast<B*>(sharedptr_.get()));
        }

        I& ref_;
        I* ptr_;
        std::shared_ptr<I> sharedptr_;
    };

    container_type container;

    container.template register_type<scope<shared>, storage<A>,
                                     interface<annotated<I, tag<1>>>>();
    container.template register_type<scope<shared>, storage<std::shared_ptr<B>>,
                                     interface<annotated<I, tag<2>>>>();
    container.template register_type<scope<shared>, storage<std::shared_ptr<C>>,
                                     interface<C, annotated<I, tag<3>>>>();

    I& aref = container.template resolve<annotated<I&, tag<1>>>();
    ASSERT_TRUE(dynamic_cast<A*>(&aref));

    I* bptr = container.template resolve<annotated<I*, tag<2>>>();
    ASSERT_TRUE(dynamic_cast<B*>(bptr));

    std::shared_ptr<I> bsharedptr =
        container.template resolve<annotated<std::shared_ptr<I>, tag<2>>>();
    ASSERT_TRUE(dynamic_cast<B*>(bsharedptr.get()));

    ASSERT_TRUE(dynamic_cast<C*>(&container.template resolve<C&>()));
    I& cref = container.template resolve<annotated<I&, tag<3>>>();
    ASSERT_TRUE(dynamic_cast<C*>(&cref));
}
} // namespace dingo