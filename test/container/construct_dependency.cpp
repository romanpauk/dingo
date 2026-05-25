//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include "container/construct_common.h"

namespace dingo {

namespace issue_43 {
struct A {};
struct B {};

template <typename... Deps> class dependencies {
  public:
    dependencies() = delete;
    dependencies(dependencies&&) = default;
    dependencies(const dependencies&) = default;
    dependencies& operator=(dependencies&&) = default;
    dependencies& operator=(const dependencies&) = default;

    explicit dependencies(Deps... deps) : deps_(std::forward<Deps>(deps)...) {}

    template <typename D> auto dependency() const { return std::get<D>(deps_); }

  private:
    std::tuple<Deps...> deps_;
};

template <typename... Deps> class auto_dependencies {
  public:
    auto_dependencies() = delete;
    auto_dependencies(auto_dependencies&&) = default;
    auto_dependencies(const auto_dependencies&) = default;
    auto_dependencies& operator=(auto_dependencies&&) = default;
    auto_dependencies& operator=(const auto_dependencies&) = default;

    explicit auto_dependencies(Deps... deps)
        : deps_(std::forward<Deps>(deps)...) {}

    template <typename D> auto dependency() const { return std::get<D>(deps_); }

  private:
    std::tuple<Deps...> deps_;
};

struct foo : dependencies<std::shared_ptr<A>, std::shared_ptr<B>> {
    using dependencies_type =
        dependencies<std::shared_ptr<A>, std::shared_ptr<B>>;
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

template <typename T> struct holder {
    T value;
};

template <typename... Deps> struct aggregate_dependencies : holder<Deps>... {
    template <typename D> auto dependency() const {
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

TYPED_TEST_SUITE(construct_dependency_test, container_types, );

TEST(construct_dependency_test, auto_constructible_trait_defaults_and_opt_in) {
    static_assert(
        std::is_aggregate_v<issue_43::aggregate_dependencies<
            std::shared_ptr<issue_43::A>, std::shared_ptr<issue_43::B>>>);
    static_assert(is_auto_constructible<issue_43::aggregate_dependencies<
                      std::shared_ptr<issue_43::A>,
                      std::shared_ptr<issue_43::B>>>::value);
    static_assert(!is_auto_constructible<
                  issue_43::dependencies<std::shared_ptr<issue_43::A>,
                                         std::shared_ptr<issue_43::B>>>::value);
    static_assert(
        is_auto_constructible<
            issue_43::auto_dependencies<std::shared_ptr<issue_43::A>,
                                        std::shared_ptr<issue_43::B>>>::value);
}

TYPED_TEST(construct_dependency_test,
           dependency_bundle_autoconstruction_requires_opt_in) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<issue_43::A>>>();
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<issue_43::B>>>();

    // Reproduces issue #43: plain non-aggregate dependency bundles still
    // require registration, but an opted-in type and the aggregate rewrite
    // auto-construct.
    ASSERT_THROW(container.template construct<issue_43::foo>(),
                 type_not_found_exception);

    auto opted_in = container.template construct<issue_43::opted_in_foo>();
    EXPECT_TRUE(opted_in.template dependency<std::shared_ptr<issue_43::A>>());
    EXPECT_TRUE(opted_in.template dependency<std::shared_ptr<issue_43::B>>());

    auto aggregate = container.template construct<issue_43::aggregate_foo>();
    EXPECT_TRUE(aggregate.template dependency<std::shared_ptr<issue_43::A>>());
    EXPECT_TRUE(aggregate.template dependency<std::shared_ptr<issue_43::B>>());
}

TYPED_TEST(construct_dependency_test,
           auto_constructible_trait_does_not_bypass_ambiguous_detection) {
    using container_type = TypeParam;

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<issue_43::A>>>();
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<issue_43::B>>>();

    ASSERT_THROW(
        container.template construct<issue_43::ambiguous_opted_in_foo>(),
        type_not_found_exception);
}

} // namespace dingo
