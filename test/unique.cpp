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
#include "test.h"

namespace dingo {
template <typename T> struct unique_test : public test<T> {};
TYPED_TEST_SUITE(unique_test, container_types);

TYPED_TEST(unique_test, value_resolve_rvalue) {
    using container_type = TypeParam;
    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class>>();
        AssertTypeNotConvertible<Class, type_list<Class*>>(container);
        {
            AssertClass(container.template resolve<Class&&>());
            ASSERT_EQ(Class::Constructor, 1);
            ASSERT_EQ(Class::MoveConstructor, 2); // TODO
            ASSERT_EQ(Class::CopyConstructor, 0);
        }

        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(unique_test, value_resolve_value) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class>>();

        {
            AssertClass(container.template resolve<Class>());
            ASSERT_EQ(Class::Constructor, 1);
            ASSERT_EQ(Class::MoveConstructor, 1); // TODO
            ASSERT_EQ(Class::CopyConstructor, 1);
        }

        { AssertClass(*container.template resolve<std::optional<Class>>()); }
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(unique_test, ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>, storage<Class*>>();
        auto c = container.template resolve<Class*>();
        AssertClass(*c);
        ASSERT_EQ(Class::Constructor, 1);
        ASSERT_EQ(Class::CopyConstructor, 0);
        ASSERT_EQ(Class::MoveConstructor, 0);
        ASSERT_EQ(Class::Destructor, 0);

        delete c;
        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(unique_test, ptr_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<Class*>,
                                     interface<IClass, IClass2>>();

    AssertClass(std::unique_ptr<IClass>(container.template resolve<IClass*>()));
    AssertClass(
        std::unique_ptr<IClass2>(container.template resolve<IClass2*>()));

    AssertClass(container.template resolve<std::unique_ptr<IClass>>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
}

TYPED_TEST(unique_test, unique_ptr) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<scope<unique>,
                                         storage<std::unique_ptr<Class>>,
                                         interface<Class, IClass>>();
        AssertTypeNotConvertible<Class, type_list<Class&, Class&&, Class*>>(
            container);
        {
            AssertClass(*container.template resolve<std::unique_ptr<Class>>());
            ASSERT_EQ(Class::Constructor, 1);
            ASSERT_EQ(Class::MoveConstructor, 0);
            ASSERT_EQ(Class::CopyConstructor, 0);
        }

        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

TYPED_TEST(unique_test, unique_ptr_multiple_interface) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<unique>, storage<std::unique_ptr<Class>>,
                                interface<IClass, IClass2>>();

    AssertClass(container.template resolve<std::unique_ptr<IClass>>());
    AssertClass(container.template resolve<std::unique_ptr<IClass2>>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(unique_test, unique_ptr_single_interface) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<
        scope<unique>, storage<std::unique_ptr<Class>>, interface<IClass>>();

    AssertClass(container.template resolve<std::unique_ptr<IClass>>());
}

TYPED_TEST(unique_test, optional) {
    using container_type = TypeParam;

    {
        container_type container;
        container.template register_type<
            scope<unique>, storage<std::optional<Class>>, interface<Class>>();
        AssertTypeNotConvertible<Class, type_list<Class&, Class&&, Class*>>(
            container);
        {
            AssertClass(*container.template resolve<std::optional<Class>>());
            ASSERT_EQ(Class::Constructor, 1);
            ASSERT_EQ(Class::MoveConstructor, 1);
            ASSERT_EQ(Class::CopyConstructor, 1); // TODO
        }

        ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
    }

    ASSERT_EQ(Class::Destructor, Class::GetTotalInstances());
}

// TODO: shared_ptr needs to be passed as &&
// TODO: creates multiple conversions. The temporaries should be tied to
//  something smaller than top-level context (perhaps per-argument in
//  constructor_detection_impl::construct?) as it is not possible to overwrite
//  the conversion, all arguments need to exist at the same time
TYPED_TEST(unique_test, unique_multiple) {
    using container_type = TypeParam;
    struct C {
        C(std::unique_ptr<IClass1>&& c1, std::unique_ptr<IClass1>&& c11,
          std::unique_ptr<IClass2>&& c2, std::unique_ptr<IClass2>&& c22) {
            AssertClass(*c1);
            AssertClass(*c11);
            assert(&c1 != &c11);
            AssertClass(*c2);
            AssertClass(*c22);
            assert(&c2 != &c22);
        }
    };

    container_type container;
    container.template register_type<scope<unique>, storage<Class*>,
                                     interface<IClass1, IClass2>>();
    container.template construct<C>();
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
