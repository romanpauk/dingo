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
#include <dingo/storage/unique.h>
#include <dingo/type_list.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct shared_cyclical_test : public test<T> {};
TYPED_TEST_SUITE(shared_cyclical_test, container_types, );

TYPED_TEST(shared_cyclical_test, recursion_exception) {
    using container_type = TypeParam;

    struct B;
    struct A {
        A(B&) {}
    };
    struct B {
        B(std::shared_ptr<A>) {}
    };

    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<A>>>();
    container.template register_type<scope<shared>, storage<B>>();

    ASSERT_THROW(container.template resolve<A>(), type_recursion_exception);
    ASSERT_THROW(container.template resolve<B>(), type_recursion_exception);
}

TYPED_TEST(shared_cyclical_test, value) {
    using container_type = TypeParam;

    struct B;
    struct A : Class {
        A(B& b) : b_(b) {}
        B& b_;
    };

    struct B : Class {
        B(A& a, IClass1& ia) : a_(a), ia_(ia) {}
        A& a_;
        IClass1& ia_;
    };

    container_type container;
    container.template register_type<scope<shared_cyclical>, storage<A>,
                                     interface<A, IClass1>>();
    container.template register_type<scope<shared_cyclical>, storage<B>>();

    auto& a = container.template resolve<A&>();
    AssertClass(a);
    AssertClass(a.b_);

    auto& b = container.template resolve<B&>();
    AssertClass(b);
    AssertClass(b.a_);
    AssertClass(b.ia_);

    auto& c = container.template resolve<IClass1&>();
    AssertClass(c);
}

TYPED_TEST(shared_cyclical_test, shared_ptr) {
    using container_type = TypeParam;

    struct B;
    struct A : Class {
        A(B& b, IClass1& ib, std::shared_ptr<IClass1>& ibptr)
            : b_(b), ib_(ib), ibptr_(ibptr) {}
        B& b_;
        IClass1& ib_;
        std::shared_ptr<IClass1>& ibptr_;
    };

    struct B : Class {
        B(A& a, IClass2& ia, std::shared_ptr<IClass2>& iaptr)
            : a_(a), ia_(ia), iaptr_(iaptr) {}
        A& a_;
        IClass2& ia_;
        std::shared_ptr<IClass2>& iaptr_;
    };

    container_type container;
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<A>>,
                                     interface<A, IClass2>>();
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<B>>,
                                     interface<B, IClass1>>();

    auto& a = container.template resolve<A&>();
    AssertClass(a);
    AssertClass(a.b_);
    AssertClass(a.ib_);
    AssertClass(*a.ibptr_);

    auto& b = container.template resolve<B&>();
    AssertClass(b);
    AssertClass(b.a_);
    AssertClass(b.ia_);
    AssertClass(*b.iaptr_);
}

// TODO: this excercises resolving code without creating temporaries,
// yet there is no way how to test it
TYPED_TEST(shared_cyclical_test, shared_ptr_interface) {
    using container_type = TypeParam;

    struct B;
    struct A : Class {
        A(IClass1& ib) : ib_(ib) {}
        IClass1& ib_;
    };

    struct B : Class {
        B(A& ia) : ia_(ia) {}
        A& ia_;
    };

    container_type container;
    container.template register_type<
        scope<shared_cyclical>, storage<std::shared_ptr<A>>, interface<A>>();
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<B>>,
                                     interface<IClass1>>();

    auto& a = container.template resolve<A&>();
    AssertClass(a);
    AssertClass(a.ib_);

    auto& b = container.template resolve<IClass1&>();
    AssertClass(b);
}

TYPED_TEST(shared_cyclical_test, exception) {
    using container_type = TypeParam;

    struct Ex {};
    struct B;
    struct A : Class {
        A(B&) { throw Ex(); }
    };

    struct B : Class {
        B(A&) {}
    };

    container_type container;
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<A>>>();
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<B>>>();

    ASSERT_THROW(container.template resolve<A&>(), Ex);
    ASSERT_THROW(container.template resolve<B&>(), Ex);
    ASSERT_THROW(container.template resolve<A&>(), Ex);
}

TYPED_TEST(shared_cyclical_test, trivially_destructible) {
    using container_type = TypeParam;

    struct B;
    struct A {
        B& b;
    };
    struct B {
        A& a;
    };

    container_type container;
    container.template register_type<scope<shared_cyclical>, storage<A>>();
    container.template register_type<scope<shared_cyclical>,
                                     storage<std::shared_ptr<B>>>();
    auto& a = container.template resolve<A&>();
    auto& b = container.template resolve<B&>();
    ASSERT_EQ(&a, &b.a);
    ASSERT_EQ(&b, &a.b);
}

TEST(shared_cyclical_test, virtual_base) {
    struct A {};
    struct B : A {};
    static_assert(!is_virtual_base_of_v<A, B>);

    struct C : virtual A {};
    static_assert(is_virtual_base_of_v<A, C>);
}

TYPED_TEST(shared_cyclical_test, ambiguous_resolve) {
    struct B;
    struct A {
        A(B& v, int, float) : b(v), index(0) {}
        A(B& v, float, int) : b(v), index(1) {}

        B& b;
        int index;
    };

    struct B {
        B(A& v, int, float) : a(v), index(0) {}
        B(A& v, float, int) : a(v), index(1) {}

        A& a;
        int index;
    };
    using container_type = TypeParam;

    {
        container_type container;

        container
            .template register_type<scope<shared_cyclical>, storage<A>,
                                    factory<constructor<A(B&, float, int)>>>();
        container
            .template register_type<scope<shared_cyclical>, storage<B>,
                                    factory<constructor<B(A&, float, int)>>>();
        container.template register_type<scope<unique>, storage<float>>();
        container.template register_type<scope<unique>, storage<int>>();

        auto& a = container.template resolve<A&>();
        ASSERT_EQ(a.index, 1);
        ASSERT_EQ(a.b.index, 1);

        auto& b = container.template resolve<B&>();
        ASSERT_EQ(b.index, 1);
        ASSERT_EQ(b.a.index, 1);
    }

    {
        container_type container;

        container
            .template register_type<scope<shared_cyclical>, storage<A>,
                                    factory<constructor<A(B&, int, float)>>>();
        container
            .template register_type<scope<shared_cyclical>, storage<B>,
                                    factory<constructor<B(A&, int, float)>>>();
        container.template register_type<scope<unique>, storage<float>>();
        container.template register_type<scope<unique>, storage<int>>();

        auto& a = container.template resolve<A&>();
        ASSERT_EQ(a.index, 0);
        ASSERT_EQ(a.b.index, 0);

        auto& b = container.template resolve<B&>();
        ASSERT_EQ(b.index, 0);
        ASSERT_EQ(b.a.index, 0);
    }
}

} // namespace dingo
