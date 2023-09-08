#include <dingo/class_factory.h>
#include <dingo/container.h>

#include <gtest/gtest.h>

#include "containers.h"

namespace dingo {

template <typename T> using test_class_factory = class_factory_wide<T, /*Assert=*/false>;

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

    static_assert(class_factory<A>::valid == false);
}

} // namespace dingo