//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct unique_test : public testing::Test {};
TYPED_TEST_SUITE(unique_test, container_types);

TYPED_TEST(unique_test, value) {
    using container_type = TypeParam;
    struct unique_value {};
    {
        typedef Class<unique_value, __COUNTER__> C;

        {
            container_type container;
            container.template register_type<scope<unique>, storage<C>>();
            AssertTypeNotConvertible<C, type_list<C*>>(container);
            {
                AssertClass(container.template resolve<C&&>());
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::MoveConstructor, 2); // TODO
                ASSERT_EQ(C::CopyConstructor, 0);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }

    {
        typedef Class<unique_value, __COUNTER__> C;

        {
            container_type container;
            container.template register_type<scope<unique>, storage<C>>();

            {
                AssertClass(container.template resolve<C>());
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::MoveConstructor, 1); // TODO
                ASSERT_EQ(C::CopyConstructor, 1);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }

    {
        typedef Class<unique_value, __COUNTER__> C;

        {
            container_type container;
            container.template register_type<scope<unique>,
                                             storage<std::unique_ptr<C>>,
                                             interface<C, IClass>>();
            AssertTypeNotConvertible<C, type_list<C&, C&&, C*>>(container);
            {
                AssertClass(*container.template resolve<std::unique_ptr<C>>());
                ASSERT_EQ(C::Constructor, 1);
                ASSERT_EQ(C::MoveConstructor, 0);
                ASSERT_EQ(C::CopyConstructor, 0);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }
}

TYPED_TEST(unique_test, ptr) {
    using container_type = TypeParam;

    struct unique_pointer {};
    /*
        // TODO: if something is stored as a pointer and is unique, we have to
       force pointer ownership transfer
        // by disallowing anything else than pointer resolution

        {
            typedef Class< unique_pointer, __COUNTER__ > C;

            {
                container_type container;
                container.template register_binding< storage< container_type,
       unique, C* >, C, IClass >(); AssertClass(container.template resolve< C
       >()); ASSERT_EQ(C::Constructor, 1); ASSERT_EQ(C::CopyConstructor, 1); //
       TODO: this is stupid. There should be no copy, just move.
                ASSERT_EQ(C::MoveConstructor, 0);
            }

            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }
    */
    {
        typedef Class<unique_pointer, __COUNTER__> C;

        {
            container_type container;
            container.template register_type<scope<unique>, storage<C*>>();
            auto c = container.template resolve<C*>();
            AssertClass(*c);
            ASSERT_EQ(C::Constructor, 1);
            ASSERT_EQ(C::CopyConstructor, 0);
            ASSERT_EQ(C::MoveConstructor, 0);
            ASSERT_EQ(C::Destructor, 0);

            delete c;
            ASSERT_EQ(C::Destructor, C::GetTotalInstances());
        }

        ASSERT_EQ(C::Destructor, C::GetTotalInstances());
    }
}

TYPED_TEST(unique_test, ptr_interface) {
    using container_type = TypeParam;

    struct unique_ptr {};
    typedef Class<unique_ptr, __COUNTER__> C;

    container_type container;

    container.template register_type<scope<unique>, storage<C*>,
                                     interface<IClass, IClass2>>();

    AssertClass(std::unique_ptr<IClass>(container.template resolve<IClass*>()));
    AssertClass(
        std::unique_ptr<IClass2>(container.template resolve<IClass2*>()));

    AssertClass(container.template resolve<std::unique_ptr<IClass>>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
}

TYPED_TEST(unique_test, unique_ptr_interface) {
    using container_type = TypeParam;

    struct unique_ptr {};
    typedef Class<unique_ptr, __COUNTER__> C;

    container_type container;

    container.template register_type<scope<unique>, storage<std::unique_ptr<C>>,
                                     interface<IClass, IClass2>>();

    AssertClass(container.template resolve<std::unique_ptr<IClass>>());
    AssertClass(container.template resolve<std::unique_ptr<IClass2>>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
}

TYPED_TEST(unique_test, ambiguous_resolve) {
    struct A {
        A() : index(0) {}
        A(int) : index(1) {}
        A(float) : index(2) {}
        A(int, float) : index(3) {}
        A(float, int) : index(4) {}

        int index;
    };

    using container_type = TypeParam;
    container_type container;

    container.template register_type<scope<unique>, storage<A>,
                                     factory<constructor<A(float, int)>>>();
    container.template register_type<scope<unique>, storage<float>>();
    container.template register_type<scope<unique>, storage<int>>();

    ASSERT_EQ((container.template construct<A, constructor<A>>().index), 0);
    ASSERT_EQ((container.template construct<A, constructor<A()>>().index), 0);
    ASSERT_EQ((container.template construct<A, constructor<A(int)>>().index),
              1);
    ASSERT_EQ((container.template construct<A, constructor<A(float)>>().index),
              2);
    ASSERT_EQ(
        (container.template construct<A, constructor<A, int, float>>().index),
        3);
    ASSERT_EQ(
        (container.template construct<A, constructor<A, float, int>>().index),
        4);
}

} // namespace dingo
