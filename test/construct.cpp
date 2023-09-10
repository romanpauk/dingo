#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"

namespace dingo {
template <typename T> struct construct_test : public testing::Test {};
TYPED_TEST_SUITE(construct_test, container_types);

TYPED_TEST(construct_test, plain) {
    using container_type = TypeParam;
    container_type container;
    container.template construct<int>();
    container.template construct<std::unique_ptr<int>>();
    container.template construct<std::shared_ptr<int>>();
    // container.template construct<std::optional<int>>(); // TODO
}

#if 0
TYPED_TEST(construct_test, ambiguous) {
    using container_type = TypeParam;
    struct A {
        A() {}
        A(int) {}
    };

    container_type container;
    container.template construct<A>();
}
#endif

TYPED_TEST(construct_test, aggregate1) {
    using container_type = TypeParam;
    struct A {
        int x;
    };
    struct B {
        B(A&&) {}
    };
    container_type container;
    container.template register_binding<storage<unique, A>>();
    container.template register_binding<storage<unique, int>>();
    container.template construct<B>();
}

TYPED_TEST(construct_test, aggregate2) {
    using container_type = TypeParam;
    struct A {
        int a = 1;
    };
    struct B {
        A a0;
    };
    struct C {
        A a0;
        A a1;
    };
    struct D {
        int a;
        int b;
    };

    container_type container;
    container.template register_binding<storage<external, int>>(2);
    container.template register_binding<storage<shared, A>>();
    auto b = container.template construct<B>();
    auto c = container.template construct<C>();
    auto d = container.template construct<D>();

    ASSERT_EQ(b.a0.a, 2);
    ASSERT_EQ(c.a0.a, 2);
    ASSERT_EQ(c.a1.a, 2);
    ASSERT_EQ(d.a, 2);
    ASSERT_EQ(d.b, 2);
}

#if 0
/*
TYPED_TEST(construct_test, resolve) {
    using container_type = TypeParam;
    struct A {};
    struct B {
        B(A&&) {}
    };
    struct C {
        C(B&) {}
    };

    container_type container;
    container.template register_binding<storage<unique, A>>();
    container.template register_binding<storage<shared, std::shared_ptr<B>>>();

    container.template construct<B>();
    container.template construct<std::unique_ptr<B>>();
    container.template construct<std::shared_ptr<B>>();
    //container.template construct<std::optional<B>>(); // TODO

    container.template construct<C>();
}
*/
#endif
} // namespace dingo
