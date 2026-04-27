//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/runtime/injector.h>
#include <dingo/runtime_container.h>
#include <dingo/runtime/registry.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>
#include <tuple>
#include <variant>

#include "support/assert.h"
#include "support/class.h"
#include "support/containers.h"
#include "support/test.h"

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

TEST(construct_test, constructor_traits) {
    struct A {};
    struct B {
        B(std::shared_ptr<A>) {}
    };

    static_assert(construction_traits<std::variant<A, B>, A>::enabled);
    static_assert(construction_traits<const std::variant<A, B>, B>::enabled);
    static_assert(!construction_traits<std::variant<A, B>, int>::enabled);
    static_assert(std::is_same_v<detail::alternative_type_alternatives_t<
                                     std::variant<A, B>>,
                                 type_list<A, B>>);
    static_assert(
        !detail::type_list_has_duplicates<detail::alternative_type_alternatives_t<
            std::variant<A, B>>>::value);
    static_assert(detail::alternative_type_count<std::variant<A, B>, A>::value == 1);
    static_assert(detail::alternative_type_count<std::variant<A, B>, B>::value == 1);
    static_assert(detail::alternative_type_count<std::variant<A, B>, int>::value == 0);
    static_assert(is_alternative_type_interface_compatible_v<std::variant<A, B>, A>);
    static_assert(is_alternative_type_interface_compatible_v<std::variant<A, B>, B>);
    static_assert(!is_alternative_type_interface_compatible_v<std::variant<A, B>, int>);

    constructor_traits<A>::construct();
    constructor_traits<B>::construct(nullptr);
    delete constructor_traits<B*>::construct(nullptr);
    constructor_traits<std::unique_ptr<B>>::construct(nullptr);
    constructor_traits<std::shared_ptr<B>>::construct(nullptr);
    constructor_traits<std::optional<B>>::construct(nullptr);
}

TEST(construct_test, auto_constructible_trait_defaults_and_opt_in) {
    static_assert(std::is_aggregate_v<issue_43::aggregate_dependencies<
                      std::shared_ptr<issue_43::A>,
                      std::shared_ptr<issue_43::B>>>);
    static_assert(is_auto_constructible<
                  issue_43::aggregate_dependencies<std::shared_ptr<issue_43::A>,
                                                   std::shared_ptr<issue_43::B>>>::value);
    static_assert(!is_auto_constructible<issue_43::dependencies<
                  std::shared_ptr<issue_43::A>,
                  std::shared_ptr<issue_43::B>>>::value);
    static_assert(is_auto_constructible<issue_43::auto_dependencies<
                  std::shared_ptr<issue_43::A>,
                  std::shared_ptr<issue_43::B>>>::value);
}

TYPED_TEST(construct_test, plain) {
    using container_type = TypeParam;
    container_type container;
    container.template construct<int>();
    container.template construct<std::unique_ptr<int>>();
    container.template construct<std::shared_ptr<int>>();
    container.template construct<std::optional<int>>();
}

TYPED_TEST(construct_test, keyed_dependencies_use_runtime_keyed_registrations) {
    using container_type = TypeParam;

    struct first_key : std::integral_constant<int, 0> {};
    struct second_key : std::integral_constant<int, 1> {};

    struct service {
        service(keyed<int&, first_key> first, keyed<int&, second_key> second)
            : sum(static_cast<int&>(first) + static_cast<int&>(second)) {}

        int sum;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>, key<first_key>>(2);
    container.template register_type<scope<external>, storage<int>, key<second_key>>(5);

    ASSERT_EQ(container.template resolve<int>(key<first_key>{}), 2);
    ASSERT_EQ(container.template resolve<int>(key<second_key>{}), 5);
    ASSERT_EQ(container.template construct<service>().sum, 7);
}

TYPED_TEST(construct_test, keyed_collection_dependencies_use_runtime_keyed_registrations) {
    using container_type = TypeParam;

    struct first_key : std::integral_constant<int, 0> {};

    struct service {
        explicit service(keyed<std::vector<std::shared_ptr<IClass>>, first_key> classes)
            : classes_(static_cast<std::vector<std::shared_ptr<IClass>>>(classes)) {}

        std::vector<std::shared_ptr<IClass>> classes_;
    };

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<0>>>,
                                     interfaces<IClass>, key<first_key>>();
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<1>>>,
                                     interfaces<IClass>, key<first_key>>();

    auto first = container.template resolve<std::vector<std::shared_ptr<IClass>>>(
        key<first_key>{});

    ASSERT_EQ(first.size(), 2);
    std::vector<size_t> first_tags{first[0]->GetTag(), first[1]->GetTag()};
    std::sort(first_tags.begin(), first_tags.end());
    ASSERT_EQ(first_tags, (std::vector<size_t>{0, 1}));
    ASSERT_THROW(
        (container.template resolve<std::shared_ptr<IClass>>(key<first_key>{})),
        type_ambiguous_exception);

    auto constructed = container.template construct<service>();
    ASSERT_EQ(constructed.classes_.size(), 2);
    std::vector<size_t> constructed_tags{constructed.classes_[0]->GetTag(),
                                         constructed.classes_[1]->GetTag()};
    std::sort(constructed_tags.begin(), constructed_tags.end());
    ASSERT_EQ(constructed_tags, (std::vector<size_t>{0, 1}));
}

TYPED_TEST(construct_test, local_bindings_resolve_local_and_runtime_dependencies) {
    using container_type = TypeParam;

    struct local_config {
        local_config() : value(7) {}
        int value;
    };

    struct local_helper {
        local_helper() : value(5) {}
        int value;
    };

    struct service {
        service(local_config& config, local_helper& helper, int runtime_value)
            : value(config.value + helper.value + runtime_value) {}

        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(3);
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<local_config>>,
                 bind<scope<shared>, storage<local_helper>>>>();

    auto& resolved = container.template resolve<service&>();

    ASSERT_EQ(resolved.value, 15);
    ASSERT_EQ(container.template construct<service>().value, 15);
}

TYPED_TEST(construct_test, local_bindings_wrapper_remains_supported) {
    using container_type = TypeParam;

    struct local_config {
        local_config() : value(4) {}
        int value;
    };

    struct service {
        service(local_config& config, int runtime_value)
            : value(config.value + runtime_value) {}

        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(3);
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<local_config>>>>();

    ASSERT_EQ(container.template construct<service>().value, 7);
}

TYPED_TEST(construct_test,
           local_bindings_override_conflicting_host_singular_dependencies) {
    using container_type = TypeParam;

    struct service {
        explicit service(int& resolved_value) : value(resolved_value) {}

        int value;
    };

    struct wrapper {
        explicit wrapper(service& resolved_service) : value(resolved_service.value) {}

        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(5);
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<int>,
                      factory<constructor<int()>>>>>();

    ASSERT_EQ(container.template resolve<service&>().value, 0);
    ASSERT_EQ(container.template construct<service>().value, 0);
    ASSERT_EQ(container.template construct<std::unique_ptr<service>>()->value, 0);
    ASSERT_EQ(container.template construct<std::shared_ptr<service>>()->value, 0);
    ASSERT_EQ(container.template construct<std::optional<service>>()->value, 0);
    ASSERT_EQ(container.template construct<wrapper>().value, 0);
}

TYPED_TEST(construct_test,
           local_bindings_override_conflicting_keyed_host_singular_dependencies) {
    using container_type = TypeParam;

    struct first_key : std::integral_constant<int, 0> {};

    struct service {
        explicit service(keyed<int&, first_key> resolved_value)
            : value(static_cast<int&>(resolved_value)) {}

        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>, key<first_key>>(5);
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<int>, key<first_key>,
                      factory<constructor<int()>>>>>();

    ASSERT_EQ(container.template resolve<service&>().value, 0);
    ASSERT_EQ(container.template construct<service>().value, 0);
}

TYPED_TEST(construct_test, local_bindings_merge_collections_with_host_bindings) {
    using container_type = TypeParam;

    struct service {
        explicit service(std::vector<std::shared_ptr<IClass>> classes)
            : classes_(std::move(classes)) {}

        std::vector<std::shared_ptr<IClass>> classes_;
    };

    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<ClassTag<1>>>,
                                     interfaces<IClass>>();
    container.template register_type<
        scope<shared>, storage<service>,
        bindings<bind<scope<shared>, storage<std::shared_ptr<ClassTag<0>>>,
                      interfaces<IClass>>>>();

    auto& resolved = container.template resolve<service&>();

    ASSERT_EQ(resolved.classes_.size(), 2);
    std::vector<size_t> tags{resolved.classes_[0]->GetTag(),
                             resolved.classes_[1]->GetTag()};
    std::sort(tags.begin(), tags.end());
    ASSERT_EQ(tags, (std::vector<size_t>{0, 1}));

    auto constructed = container.template construct<service>();
    ASSERT_EQ(constructed.classes_.size(), 2);
    std::vector<size_t> constructed_tags{constructed.classes_[0]->GetTag(),
                                         constructed.classes_[1]->GetTag()};
    std::sort(constructed_tags.begin(), constructed_tags.end());
    ASSERT_EQ(constructed_tags, (std::vector<size_t>{0, 1}));
}

TYPED_TEST(construct_test, runtime_injector_facade_uses_container_registrations) {
    using container_type = TypeParam;

    struct service {
        explicit service(int resolved_value) : value(resolved_value) {}
        int value;
    };

    container_type container;
    container.template register_type<scope<external>, storage<int>>(11);

    auto injector = container.injector();

    ASSERT_EQ(injector.template resolve<int>(), 11);
    ASSERT_EQ(injector.template construct<service>().value, 11);
}

TEST(construct_test, runtime_registry_and_runtime_container_split_runtime_source_and_facade) {
    struct service {
        explicit service(int resolved_value) : value(resolved_value) {}
        int value;
    };

    runtime_registry<> registry;
    registry.template register_type<scope<external>, storage<int>>(13);

    auto registry_injector = registry.injector();
    ASSERT_EQ(registry_injector.template resolve<int>(), 13);
    ASSERT_EQ(registry_injector.template construct<service>().value, 13);

    runtime_container<> named_container;
    named_container.template register_type<scope<external>, storage<int>>(17);

    ASSERT_EQ(named_container.template resolve<int>(), 17);
    ASSERT_EQ(named_container.template construct<service>().value, 17);
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
        container.template construct<std::variant<A, B>, constructor<A>>();
    ASSERT_TRUE(std::holds_alternative<A>(detected));
    EXPECT_EQ(std::get<A>(detected).value, 7);

    auto explicit_ctor =
        container.template construct<std::variant<A, B>, constructor<B(float)>>();
    ASSERT_TRUE(std::holds_alternative<B>(explicit_ctor));
    EXPECT_FLOAT_EQ(std::get<B>(explicit_ctor).value, 3.5f);
}

} // namespace dingo
