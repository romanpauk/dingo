//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct external_test : public test<T> {};
TYPED_TEST_SUITE(external_test, container_types, );

TYPED_TEST(external_test, value) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class>,
                                     interface<Class, IClass>>(std::move(c));

    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
    ASSERT_THROW(container.template resolve<std::optional<Class>>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, ref) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class&>,
                                     interface<Class, IClass>>(c);

    ASSERT_EQ(container.template resolve<Class*>(), &c);
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(external_test, ptr) {
    using container_type = TypeParam;
    Class c;

    container_type container;
    container.template register_type<scope<external>, storage<Class*>,
                                     interface<Class, IClass>>(&c);

    ASSERT_EQ(container.template resolve<Class*>(), &c);
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<Class>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<Class>>(),
                 type_not_convertible_exception);
    ASSERT_THROW(container.template resolve<std::shared_ptr<IClass>>(),
                 type_not_convertible_exception);
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(external_test, shared_ptr_interface) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::shared_ptr<Class>>, interface<IClass>>(c);
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
}

TYPED_TEST(external_test, shared_ptr) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>>,
                                     interface<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(*container.template resolve<Class*>());
    AssertClass(*container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>>());
    AssertClass(**container.template resolve<std::shared_ptr<Class>*>());
    AssertClass(*container.template resolve<std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>>());
    AssertClass(**container.template resolve<const std::shared_ptr<Class>*>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(*container.template resolve<std::shared_ptr<const Class>&>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>&>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
    AssertClass(*container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<const std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<std::shared_ptr<const IClass>>());
    AssertClass(
        *container.template resolve<const std::shared_ptr<const IClass>>());
}

TYPED_TEST(external_test, shared_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_shared<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>&>,
                                     interface<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(container.template resolve<std::shared_ptr<Class>>());
    AssertClass(container.template resolve<std::shared_ptr<const Class>>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>>());
    AssertClass(*container.template resolve<std::shared_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::shared_ptr<Class>&>());
    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(*container.template resolve<const std::shared_ptr<IClass>>());
    AssertClass(container.template resolve<std::shared_ptr<const IClass>>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());
}

TYPED_TEST(external_test, unique_ptr_ref) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>&>,
                                     interface<Class, IClass>>(c);
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::unique_ptr<Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
}

TYPED_TEST(external_test, unique_ptr_move) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::unique_ptr<Class>>,
                                     interface<Class, IClass>>(std::move(c));
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<const Class&>());
    AssertClass(container.template resolve<Class*>());
    AssertClass(container.template resolve<const Class*>());
    AssertClass(*container.template resolve<std::unique_ptr<Class>&>());
    AssertClass(*container.template resolve<const std::unique_ptr<Class>&>());
    AssertClass(container.template resolve<IClass&>());
    AssertClass(container.template resolve<const IClass&>());
    AssertClass(container.template resolve<IClass*>());
    AssertClass(container.template resolve<const IClass*>());

    ASSERT_THROW(container.template resolve<std::unique_ptr<IClass>&>(),
                 type_not_convertible_exception);
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(external_test, unique_ptr_interface_move) {
    using container_type = TypeParam;
    auto c = std::make_unique<Class>();

    container_type container;
    container.template register_type<
        scope<external>, storage<std::unique_ptr<Class>>, interface<IClass>>(
        std::move(c));
    AssertClass(*container.template resolve<std::unique_ptr<IClass>&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_test, optional_ref) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>&>,
                                interface<Class, IClass>>(c);
    AssertClass(*container.template resolve<std::optional<Class>&>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_test, optional_move) {
    using container_type = TypeParam;
    auto c = std::make_optional<Class>();

    container_type container;
    container
        .template register_type<scope<external>, storage<std::optional<Class>>,
                                interface<Class, IClass>>(std::move(c));
    AssertClass(*container.template resolve<std::optional<Class>&>());
    AssertClass(container.template resolve<Class&>());
    AssertClass(container.template resolve<IClass&>());
}

TYPED_TEST(external_test, shared_multiple) {
    using container_type = TypeParam;
    struct C {
        C(std::shared_ptr<IClass1>& c1, std::shared_ptr<IClass1>& c11,
          std::shared_ptr<IClass2>& c2, std::shared_ptr<IClass2>& c22) {
            AssertClass(*c1);
            AssertClass(*c11);
            assert(&c1 == &c11);
            AssertClass(*c2);
            AssertClass(*c22);
            assert(&c2 == &c22);
        }
    };

    container_type container;
    container.template register_type<scope<external>,
                                     storage<std::shared_ptr<Class>>,
                                     interface<IClass1, IClass2>>(
        std::make_shared<Class>());
    container.template construct<C>();

    {
        auto& c1 = container.template resolve<std::shared_ptr<IClass1>&>();
        AssertClass(*c1);
        auto& c2 = container.template resolve<std::shared_ptr<IClass2>&>();
        AssertClass(*c2);
        auto& c11 = container.template resolve<std::shared_ptr<IClass1>&>();
        AssertClass(*c11);
        auto& c22 = container.template resolve<std::shared_ptr<IClass2>&>();
        AssertClass(*c22);
        ASSERT_EQ(&c1, &c11);
        ASSERT_EQ(&c2, &c22);
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


TYPED_TEST(external_test, vector)
{
    using container_type = TypeParam;

    struct vector_class_ctor {
        std::vector<int> data;
        vector_class_ctor(std::vector<int> v): data(v) {}
    };

    struct vector_class_aggregate {
        std::vector<int> data;
    };

    std::vector<int> vec = {1,2,3};

    container_type container;
    container.template register_type<dingo::scope<dingo::external>, dingo::storage<std::vector<int>>>(vec);
    container.template register_type<dingo::scope<dingo::shared>, dingo::storage<vector_class_ctor>>();
    container.template register_type<dingo::scope<dingo::shared>, dingo::storage<vector_class_aggregate>>();

    {
        auto& cls = container.template resolve<vector_class_ctor&>();
        ASSERT_EQ(cls.data, vec);
    }
    {
        auto& cls = container.template resolve<vector_class_aggregate&>();
        ASSERT_EQ(cls.data, vec);
    }
}

} // namespace dingo
