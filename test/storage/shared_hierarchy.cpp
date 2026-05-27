//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "storage/shared_common.h"

namespace dingo {

TYPED_TEST_SUITE(shared_hierarchy_test, container_types, );

TYPED_TEST(shared_hierarchy_test, shared_multiple) {
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
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<Class>>,
                                interfaces<IClass1, IClass2>>();
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

TYPED_TEST(shared_hierarchy_test, hierarchy) {
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

TYPED_TEST(shared_hierarchy_test, const_hierarchy) {
    using container_type = TypeParam;

    struct S : Class {};
    struct U : Class {
        U(S& s1) { AssertClass(s1); }
    };

    struct B : Class {
        B(const S s1, const S& s2, const S* s3, const std::shared_ptr<S>* s4,
          const std::shared_ptr<S>& s5, const std::shared_ptr<S> s6, const U u1,
          const U& u2, const U* u3, const std::unique_ptr<U>* u4,
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

TYPED_TEST(shared_hierarchy_test, ambiguous_resolve) {
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
                                     factory<constructor<A(float, int)>>>();
    container.template register_type<scope<shared>, storage<float>>();
    container.template register_type<scope<shared>, storage<int>>();

    ASSERT_EQ((container.template construct<A, constructor<A()>>().index), 0);
    ASSERT_EQ((container.template construct<A, constructor<A(int)>>().index),
              1);
    ASSERT_EQ((container.template construct<A, constructor<A(float)>>().index),
              2);
    ASSERT_EQ(
        (container.template construct<A, constructor<A(int, float)>>().index),
        3);
    ASSERT_EQ(
        (container.template construct<A, constructor<A(float, int)>>().index),
        4);
}

} // namespace dingo
