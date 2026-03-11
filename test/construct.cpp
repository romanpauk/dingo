//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {
template <typename T> struct construct_test : public test<T> {};
TYPED_TEST_SUITE(construct_test, container_types, );

template <typename T> struct owned_box {
    owned_box() = default;
    explicit owned_box(T* value) : value_(value) {}

    owned_box(owned_box&&) = default;
    owned_box& operator=(owned_box&&) = default;

    owned_box(const owned_box&) = delete;
    owned_box& operator=(const owned_box&) = delete;

    T* get() const { return value_.get(); }
    T* release() { return value_.release(); }

  private:
    std::unique_ptr<T> value_;
};

struct wrapped_value {
    explicit wrapped_value(int value) : v(value) {}
    int v;
};

struct rebound_value {};

template <typename T> struct wrapper_traits<owned_box<T>> {
    using element_type = T;

    template <typename U> using rebind = owned_box<U>;
    template <typename U> using owned_rebind = owned_box<U>;
    template <typename U> using shared_rebind = std::shared_ptr<U>;
    template <typename U> using optional_rebind = void;

    static constexpr bool is_indirect = true;

    static T* get_pointer(owned_box<T>& value) { return value.get(); }

    static T* release(owned_box<T>& value) { return value.release(); }

    template <typename U> static owned_box<U> adopt(U* value) {
        return owned_box<U>(value);
    }

    template <typename... Args>
    static owned_box<T> construct(Args&&... args) {
        return adopt(new T{std::forward<Args>(args)...});
    }

    template <typename... Args>
    static void construct_at(void* ptr, Args&&... args) {
        new (ptr) owned_box<T>{construct(std::forward<Args>(args)...)};
    }
};

TEST(construct_test, wrapper_construction) {
    struct A {};
    struct B {
        B(std::shared_ptr<A>) {}
    };

    detail::construct_type<A>();
    detail::construct_type<B>(nullptr);
    delete detail::construct_type<B*>(nullptr);
    detail::construct_type<std::unique_ptr<B>>(nullptr);
    detail::construct_type<std::shared_ptr<B>>(nullptr);
    detail::construct_type<std::optional<B>>(nullptr);
}

TEST(construct_test, wrapper_traits) {
    static_assert(std::is_same_v<decay_t<owned_box<wrapped_value>>, wrapped_value>);
    static_assert(
        std::is_same_v<rebind_type_t<owned_box<wrapped_value>, rebound_value>,
                       owned_box<rebound_value>>);
    static_assert(
        std::is_same_v<rebind_type_t<const owned_box<wrapped_value>&,
                                     rebound_value>,
                       owned_box<rebound_value>&>);
    static_assert(wrapper_traits<owned_box<wrapped_value>>::is_indirect);

    auto wrapped = detail::construct_type<owned_box<wrapped_value>>(42);
    ASSERT_NE(wrapped.get(), nullptr);
    ASSERT_EQ(wrapped.get()->v, 42);
}

TYPED_TEST(construct_test, wrapper_storage_unique_resolution) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<unique>, storage<owned_box<Class>>,
                                     interfaces<IClass>>();

    auto owned = container.template resolve<owned_box<IClass>>();
    AssertClass(*owned.get());

    auto owned_const = container.template resolve<owned_box<const IClass>>();
    AssertClass(*owned_const.get());

    AssertClass(container.template resolve<std::shared_ptr<IClass>>());
    AssertClass(container.template resolve<std::shared_ptr<const IClass>>());

    AssertTypeNotConvertible<IClass, type_list<IClass*>>(container);
}

TYPED_TEST(construct_test, plain) {
    using container_type = TypeParam;
    container_type container;
    container.template construct<int>();
    container.template construct<std::unique_ptr<int>>();
    container.template construct<std::shared_ptr<int>>();
    container.template construct<std::optional<int>>();
}

#if 0
// TODO: can ambiguous construction be detected?
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
    container.template register_type<scope<unique>, storage<A>>();
    container.template register_type<scope<unique>, storage<int>>();
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
    container.template register_type<scope<external>, storage<int>>(2);
    container.template register_type<scope<unique>, storage<A>>();
    auto b = container.template construct<B>();
    auto c = container.template construct<C>();
    auto d = container.template construct<D>();

    ASSERT_EQ(b.a0.a, 2);
    ASSERT_EQ(c.a0.a, 2);
    ASSERT_EQ(c.a1.a, 2);
    ASSERT_EQ(d.a, 2);
    ASSERT_EQ(d.b, 2);
}

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
    container.template register_type<scope<unique>, storage<A>>();
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<B>>>();

    container.template construct<B>();
    container.template construct<std::unique_ptr<B>>();
    container.template construct<std::shared_ptr<B>>();
    container.template construct<std::optional<B>>();

    container.template construct<C>();
}

} // namespace dingo
