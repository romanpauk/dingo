#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "containers.h"

namespace dingo {
template <typename T> struct class_factory_test : public testing::Test {};
TYPED_TEST_SUITE(class_factory_test, container_types);

template <typename T> using test_class_factory = detail::constructor_detection<T, /*Assert=*/false>;

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

TEST(class_factory_test, constructor) {
    struct A {
        A(int, float);
    };

    static_assert(constructor<A, int, float>::valid && constructor<A(int, float)>::valid);
    static_assert(constructor<A, int, float>::arity && constructor<A(int, float)>::arity);
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
        auto a = container.template construct<A, constructor<A(int)>>();
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
    {
        auto a = container.template construct<A>(constructor<A, float, int>());
        ASSERT_EQ(a.index, 3);
    }
}

#if defined(__GNUC__)
// TODO:
// Investigate https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83258
struct A {
    static A createInt(int v) { return A(v); }
    static A createFloat(float v) { return A(v); }

    int index;

  private:
    A(int) { index = 0; };
    A(float) { index = 1; };
};
#endif

TYPED_TEST(class_factory_test, function_construct) {
#if !defined(__GNUC__)
    struct A {
        static A createInt(int v) { return A(v); }
        static A createFloat(float v) { return A(v); }

        int index;

      private:
        A(int) { index = 0; };
        A(float) { index = 1; };
    };
#endif
    using container_type = TypeParam;
    container_type container;
    container.template register_binding<storage<unique, int>>();
    container.template register_binding<storage<unique, float>>();

    {
        auto a = container.template construct<A, function_decl<decltype(&A::createInt), &A::createInt>>();
        ASSERT_EQ(a.index, 0);
    }
    {
        auto a = container.template construct<A>(function_decl<decltype(&A::createInt), &A::createInt>());
        ASSERT_EQ(a.index, 0);
    }

#if !defined(__GNUC__) || __GNUC__ >= 12
    {
        auto a = container.template construct<A, function_decl<decltype(A::createFloat), A::createFloat>>();
        ASSERT_EQ(a.index, 1);
    }
    {
        auto a = container.template construct<A>(function_decl<decltype(A::createFloat), A::createFloat>());
        ASSERT_EQ(a.index, 1);
    }

    {
        auto a = container.template construct<A, function<&A::createFloat>>();
        ASSERT_EQ(a.index, 1);
    }
    {
        auto a = container.template construct<A>(function<&A::createFloat>());
        ASSERT_EQ(a.index, 1);
    }
#endif
}

TYPED_TEST(class_factory_test, callable_construct) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_binding<storage<external, int>>(4);
    auto b = container.template construct<B>(callable([&](int v) { return B{v * v}; }));
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_resolve_unique) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_binding<storage<external, int>>(4);
    container.template register_binding<storage<unique, B>>(callable([&](int v) { return B{v * v}; }));
    auto b = container.template resolve<B>();
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_resolve_shared) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_binding<storage<external, int>>(4);
    container.template register_binding<storage<shared, B>>(callable([&](int v) { return B{v * v}; }));
    auto b = container.template resolve<B>();
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_resolve_shared_cyclical) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_binding<storage<external, int>>(4);
    container.template register_binding<storage<shared_cyclical, B, constructor<B>, container_type>>(
        callable([&](int v) { return B{v * v}; }));
    auto b = container.template resolve<B&>();
    ASSERT_EQ(b.v, 16);
}

} // namespace dingo