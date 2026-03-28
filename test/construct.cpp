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

#include <tuple>
#include <variant>

#include "assert.h"
#include "class.h"
#include "containers.h"
#include "test.h"

namespace dingo {

namespace issue_43 {
struct A {};
struct B {};

template <typename... Deps>
class dependencies {
  public:
    dependencies() = delete;
    dependencies(dependencies&&) = default;
    dependencies(const dependencies&) = default;
    dependencies& operator=(dependencies&&) = default;
    dependencies& operator=(const dependencies&) = default;

    explicit dependencies(Deps... deps) : deps_(std::forward<Deps>(deps)...) {}

    template <typename D>
    auto dependency() const {
        return std::get<D>(deps_);
    }

  private:
    std::tuple<Deps...> deps_;
};

template <typename... Deps>
class auto_dependencies {
  public:
    auto_dependencies() = delete;
    auto_dependencies(auto_dependencies&&) = default;
    auto_dependencies(const auto_dependencies&) = default;
    auto_dependencies& operator=(auto_dependencies&&) = default;
    auto_dependencies& operator=(const auto_dependencies&) = default;

    explicit auto_dependencies(Deps... deps)
        : deps_(std::forward<Deps>(deps)...) {}

    template <typename D>
    auto dependency() const {
        return std::get<D>(deps_);
    }

  private:
    std::tuple<Deps...> deps_;
};

struct foo : dependencies<std::shared_ptr<A>, std::shared_ptr<B>> {
    using dependencies_type = dependencies<std::shared_ptr<A>, std::shared_ptr<B>>;
    using dependencies_type::dependency;

    explicit foo(dependencies_type deps) : dependencies_type(std::move(deps)) {}
};

struct opted_in_foo
    : auto_dependencies<std::shared_ptr<A>, std::shared_ptr<B>> {
    using dependencies_type =
        auto_dependencies<std::shared_ptr<A>, std::shared_ptr<B>>;
    using dependencies_type::dependency;

    explicit opted_in_foo(dependencies_type deps)
        : dependencies_type(std::move(deps)) {}
};

struct ambiguous_auto_dependencies {
    ambiguous_auto_dependencies(std::shared_ptr<A>) {}
    ambiguous_auto_dependencies(std::shared_ptr<B>) {}
};

struct ambiguous_opted_in_foo {
    using dependencies_type = ambiguous_auto_dependencies;

    explicit ambiguous_opted_in_foo(dependencies_type) {}
};

template <typename T>
struct holder {
    T value;
};

template <typename... Deps>
struct aggregate_dependencies : holder<Deps>... {
    template <typename D>
    auto dependency() const {
        static_assert((std::is_same_v<D, Deps> || ...));
        return static_cast<const holder<D>&>(*this).value;
    }
};

struct aggregate_foo
    : aggregate_dependencies<std::shared_ptr<A>, std::shared_ptr<B>> {
    using dependencies_type =
        aggregate_dependencies<std::shared_ptr<A>, std::shared_ptr<B>>;
    using dependencies_type::dependency;

    explicit aggregate_foo(dependencies_type deps)
        : dependencies_type(std::move(deps)) {}
};
} // namespace issue_43

template <typename... Deps>
struct is_auto_constructible<issue_43::auto_dependencies<Deps...>>
    : std::true_type {};
template <>
struct is_auto_constructible<issue_43::ambiguous_auto_dependencies>
    : std::true_type {};

template <typename T> struct construct_test : public test<T> {};
TYPED_TEST_SUITE(construct_test, container_types, );

TEST(construct_test, class_traits) {
    struct A {};
    struct B {
        B(std::shared_ptr<A>) {}
    };

    static_assert(construction_traits<std::variant<A, B>, A>::enabled);
    static_assert(!construction_traits<std::variant<A, B>, int>::enabled);

    class_traits<A>::construct();
    class_traits<B>::construct(nullptr);
    delete class_traits<B*>::construct(nullptr);
    class_traits<std::unique_ptr<B>>::construct(nullptr);
    class_traits<std::shared_ptr<B>>::construct(nullptr);
    class_traits<std::optional<B>>::construct(nullptr);
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

TYPED_TEST(construct_test, dependency_bundle_autoconstruction_requires_opt_in) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<shared>,
                                storage<std::shared_ptr<issue_43::A>>>();
    container
        .template register_type<scope<shared>,
                                storage<std::shared_ptr<issue_43::B>>>();

    // Reproduces issue #43: plain non-aggregate dependency bundles still
    // require registration, but an opted-in type and the aggregate rewrite
    // auto-construct.
    ASSERT_THROW(container.template construct<issue_43::foo>(),
                 type_not_found_exception);

    auto opted_in = container.template construct<issue_43::opted_in_foo>();
    EXPECT_TRUE(
        opted_in.template dependency<std::shared_ptr<issue_43::A>>());
    EXPECT_TRUE(
        opted_in.template dependency<std::shared_ptr<issue_43::B>>());

    auto aggregate = container.template construct<issue_43::aggregate_foo>();
    EXPECT_TRUE(
        aggregate.template dependency<std::shared_ptr<issue_43::A>>());
    EXPECT_TRUE(
        aggregate.template dependency<std::shared_ptr<issue_43::B>>());
}

TYPED_TEST(construct_test, auto_constructible_trait_does_not_bypass_ambiguous_detection) {
    using container_type = TypeParam;

    container_type container;
    container
        .template register_type<scope<shared>,
                                storage<std::shared_ptr<issue_43::A>>>();
    container
        .template register_type<scope<shared>,
                                storage<std::shared_ptr<issue_43::B>>>();

    ASSERT_THROW(
        container.template construct<issue_43::ambiguous_opted_in_foo>(),
        type_not_found_exception);
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

TYPED_TEST(construct_test, variant_selected_alternative) {
    using container_type = TypeParam;

    struct A {
        explicit A(int init) : value(init) {}
        int value;
    };
    struct B {
        explicit B(float init) : value(init) {}
        float value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(7);
    container.template register_type<scope<external>, storage<float>>(3.5f);

    auto detected =
        container.template construct<std::variant<A, B>,
                                     constructor_detection<A>>();
    ASSERT_TRUE(std::holds_alternative<A>(detected));
    EXPECT_EQ(std::get<A>(detected).value, 7);

    auto explicit_ctor =
        container.template construct<std::variant<A, B>, constructor<B(float)>>();
    ASSERT_TRUE(std::holds_alternative<B>(explicit_ctor));
    EXPECT_FLOAT_EQ(std::get<B>(explicit_ctor).value, 3.5f);
}

} // namespace dingo
