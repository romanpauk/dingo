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

// TODO: does not compile
#if 0
TYPED_TEST(unique_test, value_interface) {
    using container_type = TypeParam;
    struct unique_value {};
    typedef Class<unique_value, __COUNTER__> C;
    container_type container;
    container.template register_type<scope<unique>, storage<C>, interface<IClass>>();
}
#endif

TYPED_TEST(unique_test, pointer) {
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

TYPED_TEST(unique_test, ambiguous_resolve) {
    struct A {
        A(int) : index(0) {}
        A(float) : index(1) {}
        A(int, float) : index(2) {}
        A(float, int) : index(3) {}

        int index;
    };

    using container_type = TypeParam;

    using container_type = TypeParam;
    container_type container;

    container.template register_type<scope<unique>, storage<A>,
                                     factory<constructor<A(float, int)>>>();
    container.template register_type<scope<unique>, storage<float>>();
    container.template register_type<scope<unique>, storage<int>>();

    auto a = container.template construct<A, constructor<A, float, int>>();
    ASSERT_EQ(a.index, 3);
}

} // namespace dingo
