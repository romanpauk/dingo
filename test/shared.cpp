//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/factory/constructor.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct shared_test : public test<T> {};
TYPED_TEST_SUITE(shared_test, container_types);

TYPED_TEST(shared_test, value) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class>,
                                         interface<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());

        AssertTypeNotConvertible<
            Class, type_list<std::shared_ptr<Class>, std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, value_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>, storage<Class>,
                                     interface<IClass>>();

    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<IClass*>());
}

TYPED_TEST(shared_test, ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>, storage<Class*>,
                                         interface<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());

        AssertTypeNotConvertible<
            Class, type_list<std::shared_ptr<Class>, std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>, storage<Class*>,
                                     interface<IClass>>();

    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<IClass*>());
}

TYPED_TEST(shared_test, shared_ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<shared>,
                                         storage<std::shared_ptr<Class>>,
                                         interface<Class, IClass>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
        AssertClass(**container.template resolve<std::shared_ptr<Class>*>());

        AssertTypeNotConvertible<Class, type_list<std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

TYPED_TEST(shared_test, shared_ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<Class>>,
                                interface<Class, IClass>>();

    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(container.template resolve<std::shared_ptr<Class>>());
}

TYPED_TEST(shared_test, unique_ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<shared>, storage<std::unique_ptr<Class>>, interface<Class>>();

        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
        AssertClass(**container.template resolve<std::unique_ptr<Class>*>());

        AssertTypeNotConvertible<Class, type_list<std::unique_ptr<Class>>>(
            container);

        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::Destructor, 0);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
    }

    { ASSERT_EQ(Class::Destructor, Class::GetTotalInstances()); }
}

#if 0
// TODO: if IClass is single interface and has virtual dtor, the storage could
// be defined with the interface type. Doable, but rather big change for this
// only case if implemented.
TYPED_TEST(shared_test, unique_ptr_interface) {
    using container_type = TypeParam;
    
    container_type container;
    container.template register_type<scope<shared>,
                                        storage<std::unique_ptr<Class>>, interface<IClass>>();
}
#endif

TYPED_TEST(shared_test, hierarchy) {
    using container_type = TypeParam;

    struct S : Class {};
    struct U : Class {
        U(S& s1) { AssertClass(s1); }
    };

    struct B : Class {
        B(S s1, S& s2, S* s3, std::shared_ptr<S>* s4, std::shared_ptr<S>& s5,
          std::shared_ptr<S> s6, U u1, U& u2, U* u3, std::unique_ptr<U>* u4,
          std::unique_ptr<U>& u5) {
            AssertClass(s1);
            AssertClass(s2);
            AssertClass(*s3);
            AssertClass(**s4);
            AssertClass(*s5);
            AssertClass(*s6);

            AssertClass(u1);
            AssertClass(u2);
            AssertClass(*u3);
            AssertClass(**u4);
            AssertClass(*u5);
        }
    };

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<S>>>();
    container
        .template register_type<scope<shared>, storage<std::unique_ptr<U>>>();
    container.template register_type<scope<shared>, storage<B>>();

    container.template resolve<B&>();
}

TYPED_TEST(shared_test, ambiguous_resolve) {
    struct A {
        A() : index(0) {}
        A(int) : index(1) {}
        A(float) : index(2) {}
        A(int, float) : index(3) {}
        A(float, int) : index(4) {}

        int index;
    };

    using container_type = TypeParam;

    using container_type = TypeParam;
    container_type container;

    container.template register_type<scope<shared>, storage<A>,
                                     factory<constructor<A, float, int>>>();
    container.template register_type<scope<shared>, storage<float>>();
    container.template register_type<scope<shared>, storage<int>>();

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
