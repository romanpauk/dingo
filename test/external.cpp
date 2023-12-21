//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct external_test : public test<T> {};
TYPED_TEST_SUITE(external_test, container_types);

TYPED_TEST(external_test, value) {
    using container_type = TypeParam;
    Class c;

    {
        container_type container;
        container.template register_type<scope<external>, storage<Class>,
                                         interface<Class, IClass>>(
            std::move(c));

        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<IClass&>());
        AssertClass(container.template resolve<IClass*>());
    }
}

TYPED_TEST(external_test, ref) {
    using container_type = TypeParam;
    Class c;

    {
        container_type container;
        container.template register_type<scope<external>, storage<Class&>,
                                         interface<Class, IClass>>(c);

        ASSERT_EQ(container.template resolve<Class*>(), &c);
        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<IClass&>());
        AssertClass(container.template resolve<IClass*>());
    }
}

TYPED_TEST(external_test, ptr) {
    using container_type = TypeParam;
    Class c;

    {
        container_type container;
        container.template register_type<scope<external>, storage<Class*>,
                                         interface<Class, IClass>>(&c);

        ASSERT_EQ(container.template resolve<Class*>(), &c);
        AssertClass(*container.template resolve<Class*>());
        AssertClass(container.template resolve<Class&>());
        AssertClass(container.template resolve<IClass&>());
        AssertClass(container.template resolve<IClass*>());
    }
}

TYPED_TEST(external_test, shared_ptr) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    {
        container_type container;
        container.template register_type<scope<external>,
                                         storage<std::shared_ptr<Class>>,
                                         interface<Class, IClass>>(c);
        AssertClass(container.template resolve<Class&>());
        AssertClass(*container.template resolve<Class*>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>>());
        AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
        AssertClass(container.template resolve<IClass&>());
        AssertClass(container.template resolve<IClass*>());
        AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
    }
}

TYPED_TEST(external_test, shared_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    {
        container_type container;
        container.template register_type<scope<external>,
                                         storage<std::shared_ptr<Class>&>,
                                         interface<Class, IClass>>(c);
        container.template resolve<Class&>();
        container.template resolve<Class*>();
        container.template resolve<std::shared_ptr<Class>>();
        container.template resolve<std::shared_ptr<Class>&>();
        container.template resolve<std::shared_ptr<IClass>>();
        container.template resolve<IClass&>();
    }
}

TYPED_TEST(external_test, unique_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    {
        container_type container;
        container.template register_type<scope<external>,
                                         storage<std::unique_ptr<Class>&>
                                         // interface<C, IClass> // TODO
                                         >(c);
        container.template resolve<Class&>();
        container.template resolve<Class*>();
        container.template resolve<std::unique_ptr<Class>&>();
        // container.template resolve<IClass&>();
    }
}

TYPED_TEST(external_test, unique_ptr_move) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    {
        container_type container;
        container.template register_type<scope<external>,
                                         storage<std::unique_ptr<Class>>
                                         // , interface<C, IClass> // TODO
                                         >(std::move(c));
        container.template resolve<Class&>();
        container.template resolve<Class*>();
        container.template resolve<std::unique_ptr<Class>&>();
        // container.template resolve<IClass&>();
    }
}

TYPED_TEST(external_test, constructor_ambiguous) {
    using container_type = TypeParam;
    container_type container;

    struct A {
        A(int) {}
        A(double) {}
        static A& instance() {
            static A a(1);
            return a;
        }
    };

    container.template register_type<scope<external>, storage<A>>(
        A::instance());
}

TYPED_TEST(external_test, constructor_private) {
    using container_type = TypeParam;
    container_type container;

    struct A {
        static A& instance() {
            static A a;
            return a;
        }

      private:
        A() {}
    };

    container.template register_type<scope<external>, storage<A>>(
        A::instance());
}

} // namespace dingo
