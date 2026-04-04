//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/registration/constructor.h>
#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/function.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type/type_list.h>

#include <gtest/gtest.h>

#include <functional>

#include "support/containers.h"
#include "support/test.h"

namespace dingo {
template <typename T> struct class_factory_test : public test<T> {};
TYPED_TEST_SUITE(class_factory_test, container_types, );
using constructor_kind = detail::constructor_kind;

template <typename T>
using test_class_factory =
    detail::constructor_detection<T, detail::automatic,
                                  detail::list_initialization>;

struct generic_constructor_factory_test_type {
    template <class U, class V> generic_constructor_factory_test_type(U, V) {}
};

struct mixed_constructor_factory_test_type {
    mixed_constructor_factory_test_type(int) {}

    template <class U, class V> mixed_constructor_factory_test_type(U, V) {}
};

struct explicit_generic_constructor_factory_test_type {
    template <class U, class V>
    explicit_generic_constructor_factory_test_type(U, V) {
        index = std::is_same_v<std::decay_t<U>, int> &&
                        std::is_same_v<std::decay_t<V>, float>
                    ? 0
                    : -1;
    }

    int index = -1;
};

TEST(class_factory_test, default_constructor) {
    struct A {
        A() = default;
    };
    static_assert(test_class_factory<A>::arity == 0 &&
                  test_class_factory<A>::kind == constructor_kind::concrete);
}

TEST(class_factory_test, default_constructor_delete) {
    struct A {
        A() = delete;
    };
#if DINGO_CXX_STANDARD <= 17
    [[maybe_unused]] A a{};
    static_assert(test_class_factory<A>::arity == 0 &&
                  test_class_factory<A>::kind == constructor_kind::concrete);
#else
    static_assert(test_class_factory<A>::kind == constructor_kind::invalid);
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
    static_assert(test_class_factory<A>::arity == 2 &&
                  test_class_factory<A>::kind == constructor_kind::concrete);

    struct A1 {
        A1(int) {}
        int a;
        int b;
    };
    static_assert(test_class_factory<A1>::arity == 1 &&
                  test_class_factory<A1>::kind == constructor_kind::concrete);
}

// This case is skipped in constructability detection
// as copy constructor needs to be skipped.
TEST(class_factory_test, unconstructible) {
    struct A {
        A& a;
    };

    static_assert(test_class_factory<A>::kind == constructor_kind::invalid);
}

TEST(class_factory_test, constructor) {
    struct A {
        A(int, float);
    };

    static_assert(constructor<A, int, float>::valid ==
                  constructor<A(int, float)>::valid);
    static_assert(constructor<A, int, float>::arity ==
                  constructor<A(int, float)>::arity);
}

TEST(class_factory_test, generic_constructor_requires_explicit_factory) {
    static_assert(test_class_factory<generic_constructor_factory_test_type>::arity ==
                  2);
    static_assert(test_class_factory<generic_constructor_factory_test_type>::kind ==
                  constructor_kind::generic);
    static_assert(
        constructor<generic_constructor_factory_test_type, int, float>::valid);
}

TEST(class_factory_test, mixed_constructor_requires_explicit_factory) {
    static_assert(test_class_factory<mixed_constructor_factory_test_type>::arity ==
                  2);
    static_assert(test_class_factory<mixed_constructor_factory_test_type>::kind ==
                  constructor_kind::generic);
    static_assert(constructor<mixed_constructor_factory_test_type, int>::valid);
    static_assert(
        constructor<mixed_constructor_factory_test_type, int, float>::valid);
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
    static_assert(test_class_factory<A>::kind == constructor_kind::invalid);

    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<unique>, storage<float>>();

    {
        static_assert(constructor<A, int>::arity == 1 &&
                      constructor<A, int>::valid == true);
        auto a = container.template construct<A, constructor<A(int)>>();
        ASSERT_EQ(a.index, 0);
    }

    {
        static_assert(constructor<A, float>::arity == 1 &&
                      constructor<A, float>::valid == true);
        auto a = container.template construct<A, constructor<A, float>>();
        ASSERT_EQ(a.index, 1);
    }

    {
        static_assert(constructor<A, int, float>::arity == 2 &&
                      constructor<A, int, float>::valid == true);
        auto a = container.template construct<A, constructor<A, int, float>>();
        ASSERT_EQ(a.index, 2);
    }

    {
        static_assert(constructor<A, int, float>::arity == 2 &&
                      constructor<A, float, int>::valid == true);
        auto a = container.template construct<A, constructor<A, float, int>>();
        ASSERT_EQ(a.index, 3);
    }
    {
        auto a = container.template construct<A>(constructor<A, float, int>());
        ASSERT_EQ(a.index, 3);
    }
}

TYPED_TEST(class_factory_test, explicit_factory_constructs_generic_constructor) {
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);
    container.template register_type<scope<external>, storage<float>>(1.5f);
    container.template register_type<
        scope<unique>, storage<explicit_generic_constructor_factory_test_type>,
        factory<constructor<explicit_generic_constructor_factory_test_type(
            int, float)>>>();

    auto a =
        container.template resolve<explicit_generic_constructor_factory_test_type>();
    ASSERT_EQ(a.index, 0);

    auto direct = container.template construct<
        explicit_generic_constructor_factory_test_type,
        constructor<explicit_generic_constructor_factory_test_type(int, float)>>();
    ASSERT_EQ(direct.index, 0);
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
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<unique>, storage<float>>();

    {
        auto a = container.template construct<
            A, function_decl<decltype(&A::createInt), &A::createInt>>();
        ASSERT_EQ(a.index, 0);
    }
    {
        auto a = container.template construct<A>(
            function_decl<decltype(&A::createInt), &A::createInt>());
        ASSERT_EQ(a.index, 0);
    }

#if !defined(__GNUC__) || __GNUC__ >= 12
    {
        auto a = container.template construct<
            A, function_decl<decltype(A::createFloat), A::createFloat>>();
        ASSERT_EQ(a.index, 1);
    }
    {
        auto a = container.template construct<A>(
            function_decl<decltype(A::createFloat), A::createFloat>());
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
    container.template register_type<scope<external>, storage<int>>(4);
    auto b = container.template construct<B>(
        callable([&](int v) { return B{v * v}; }));
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_resolve_unique) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);
    container.template register_type<scope<unique>, storage<B>>(
        callable([&](int v) { return B{v * v}; }));
    auto b = container.template resolve<B>();
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_resolve_shared) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);
    container.template register_type<scope<shared>, storage<B>>(
        callable([&](int v) { return B{v * v}; }));
    auto b = container.template resolve<B>();
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_resolve_shared_cyclical) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);
    container.template register_type<scope<shared_cyclical>, storage<B>>(
        callable([&](int v) { return B{v * v}; }));
    auto b = container.template resolve<B&>();
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_construct_mutable_lambda) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);

    auto fn = [factor = 1](int v) mutable {
        factor += v;
        return B{factor};
    };

    auto first = container.template construct<B>(callable(fn));
    auto second = container.template construct<B>(callable(fn));
    ASSERT_EQ(first.v, 5);
    ASSERT_EQ(second.v, 5);
}

#if defined(__cpp_lib_move_only_function)
TYPED_TEST(class_factory_test, callable_construct_move_only_function) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);

    std::move_only_function<B(int)> fn = [](int v) { return B{v * v}; };
    auto b = container.template construct<B>(callable(std::move(fn)));
    ASSERT_EQ(b.v, 16);
}

TYPED_TEST(class_factory_test, callable_resolve_unique_move_only_function) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);
    container.template register_type<scope<unique>, storage<B>>(
        callable(std::move_only_function<B(int)>(
            [](int v) { return B{v * 2}; })));

    ASSERT_EQ(container.template resolve<B>().v, 8);
    ASSERT_EQ(container.template resolve<B>().v, 8);
}
#endif

#if defined(__cpp_lib_copyable_function)
TYPED_TEST(class_factory_test, callable_construct_copyable_function) {
    struct B {
        int v;
    };
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(4);

    std::copyable_function<B(int)> fn = [](int v) { return B{v + 3}; };
    auto b = container.template construct<B>(callable(fn));
    ASSERT_EQ(b.v, 7);
}
#endif

TYPED_TEST(class_factory_test, callable_construct_explicit_signature_overloaded_functor) {
    struct B {
        int v;
    };

    struct overloaded {
        B operator()(int value) const { return B{value + 10}; }
        B operator()(float value) const { return B{static_cast<int>(value) + 100}; }
    };

    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(7);
    container.template register_type<scope<external>, storage<float>>(1.5f);

    auto from_int =
        container.template construct<B>(callable<B(int)>(overloaded{}));
    auto from_float =
        container.template construct<B>(callable<B(float)>(overloaded{}));

    ASSERT_EQ(from_int.v, 17);
    ASSERT_EQ(from_float.v, 101);
}

TYPED_TEST(class_factory_test, callable_resolve_unique_explicit_signature_overloaded_functor) {
    struct B {
        int v;
    };

    struct overloaded {
        B operator()(int value) const { return B{value + 10}; }
        B operator()(float value) const { return B{static_cast<int>(value) + 100}; }
    };

    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(7);
    container.template register_type<scope<unique>, storage<B>>(
        callable<B(int)>(overloaded{}));

    ASSERT_EQ(container.template resolve<B>().v, 17);
    ASSERT_EQ(container.template resolve<B>().v, 17);
}

#if DINGO_CXX_STANDARD >= 20
TYPED_TEST(class_factory_test, callable_construct_explicit_signature_generic_lambda) {
    struct B {
        int v;
    };

    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(6);

    auto fn = []<typename T>(T value) { return B{value * 2}; };

    auto b = container.template construct<B>(callable<B(int)>(fn));
    ASSERT_EQ(b.v, 12);
}

TYPED_TEST(class_factory_test, callable_resolve_unique_explicit_signature_generic_lambda) {
    struct B {
        int v;
    };

    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<external>, storage<int>>(6);

    auto fn = []<typename T>(T value) { return B{value * 2}; };

    container.template register_type<scope<unique>, storage<B>>(
        callable<B(int)>(fn));
    ASSERT_EQ(container.template resolve<B>().v, 12);
    ASSERT_EQ(container.template resolve<B>().v, 12);
}
#endif

TYPED_TEST(class_factory_test, constructor_type) {
    struct B {
        B(int) : index(0) {}
        DINGO_CONSTRUCTOR(B(int, int)) : index(1) {}
        B(int, int, int) : index(2) {}
        int index = 0;
    };
    static_assert(constructor_detection<B>::kind ==
                  constructor_kind::concrete);
    using container_type = TypeParam;
    container_type container;
    container.template register_type<scope<unique>, storage<int>>();
    container.template register_type<scope<unique>, storage<B>>();
    ASSERT_EQ(container.template resolve<B>().index, 1);
}

} // namespace dingo
