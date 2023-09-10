#include <dingo/class_factory.h>
#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "containers.h"

namespace dingo {
template <typename T> struct class_factory_test : public testing::Test {};
TYPED_TEST_SUITE(class_factory_test, container_types);

template <typename T> using test_class_factory = class_factory<T, void, /*Assert=*/false>;

TEST(class_factory_test, default_constructor) {
    struct A {
        A() = default;
    };
    static_assert(test_class_factory<A>::arity == 0 && test_class_factory<A>::valid == true);
}

TEST(class_factory_test, default_constructor_delete) {
    struct A {
        A() = delete;
    };
#if DINGO_CXX_STANDARD <= DINGO_CXX17
    A a{};
    static_assert(test_class_factory<A>::arity == 0 && test_class_factory<A>::valid == true);
#else
    static_assert(test_class_factory<A>::valid == false);
#endif
}

TEST(class_factory_test, constructor_ambiguous) {
    struct A {
        A() {}
        A(const int) {}
    };
    static_assert(test_class_factory<A>::arity == 1);
}

TEST(class_factory_test, aggregate) {
    struct A {
        int a;
        int b;
    };
    static_assert(test_class_factory<A>::arity == 2 && test_class_factory<A>::valid == true);

    struct A1 {
        A1(int) {}
        int a;
        int b;
    };
    static_assert(test_class_factory<A1>::arity == 1 && test_class_factory<A1>::valid == true);
}

// This case is skipped in constructability detection
// as copy constructor needs to be skipped.
TEST(class_factory_test, unconstructible) {
    struct A {
        A& a;
    };

    static_assert(test_class_factory<A>::valid == false);
}

TYPED_TEST(class_factory_test, ambiguous_construct) {
    struct A {
        A(int) : index(0) {}
        A(float) : index(1) {}
        A(int, float) : index(2) {}
        A(float, int) : index(3) {}

        int index;
    };

    static_assert(constructor<A, double>::valid == false);
    static_assert(test_class_factory<A>::valid == false);

    using container_type = TypeParam;
    container_type container;
    container.template register_binding<storage<unique, int>>();
    container.template register_binding<storage<unique, float>>();

    {
        static_assert(constructor<A, int>::arity == 1 && constructor<A, int>::valid == true);
        auto a = container.template construct<A, constructor<A, int>>();
        ASSERT_EQ(a.index, 0);
    }

    {
        static_assert(constructor<A, float>::arity == 1 && constructor<A, float>::valid == true);
        auto a = container.template construct<A, constructor<A, float>>();
        ASSERT_EQ(a.index, 1);
    }

    {
        static_assert(constructor<A, int, float>::arity == 2 && constructor<A, int, float>::valid == true);
        auto a = container.template construct<A, constructor<A, int, float>>();
        ASSERT_EQ(a.index, 2);
    }

    {
        static_assert(constructor<A, int, float>::arity == 2 && constructor<A, float, int>::valid == true);
        auto a = container.template construct<A, constructor<A, float, int>>();
        ASSERT_EQ(a.index, 3);
    }
}

} // namespace dingo