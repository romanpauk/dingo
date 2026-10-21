//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Runtime registrations of the next implementation: what a registration keeps
// and for how long, what a failed request of the container undoes, and how
// the local bindings of a registration compose with the container's.
// spec: transaction_rules

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/function.h>
#include <dingo/lookup/lookup.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

using dingo::scope;
using dingo::storage;

// Counts the live objects of a type, and the destructions of each.
template <int Id> struct counted {
  counted() { ++live; }
  counted(const counted &) { ++live; }
  counted(counted &&) noexcept { ++live; }
  ~counted() {
    alive = false;
    ++destructions;
    --live;
  }
  bool alive = true;
  static inline int live = 0;
  static inline int destructions = 0;
  static void clear() { live = destructions = 0; }
};

struct static_rtti_traits {
  using rtti_type = dingo::rtti<dingo::static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

template <typename Container>
class next_runtime_registration_borrowing : public ::testing::Test {};

using container_types =
    ::testing::Types<dingo::container<>,
                     dingo::container<dingo::static_container_traits>,
                     dingo::container<static_rtti_traits>>;

TYPED_TEST_SUITE(next_runtime_registration_borrowing, container_types, );

// An external binding borrows the object a raw pointer points to: it serves
// the pointer and the object, and never adopts the object into an owning
// handle, alone or inside an optional.
TYPED_TEST(next_runtime_registration_borrowing,
           an_external_pointer_is_never_adopted_by_an_owning_handle) {
  int value = 7;
  TypeParam container;
  container.template register_type<scope<dingo::external>, storage<int *>>(
      &value);
  EXPECT_EQ(container.template resolve<int *>(), &value);
  EXPECT_EQ(&container.template resolve<int &>(), &value);
  EXPECT_THROW((void)container.template resolve<std::shared_ptr<int>>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW((void)container.template resolve<std::shared_ptr<int> &>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW((void)container.template resolve<std::unique_ptr<int>>(),
               dingo::type_not_convertible_exception);
  EXPECT_THROW(
      (void)container.template resolve<std::optional<std::shared_ptr<int>>>(),
      dingo::type_not_convertible_exception);
}

TYPED_TEST(next_runtime_registration_borrowing,
           a_pointer_in_an_external_optional_is_never_adopted) {
  auto value = std::make_unique<int>(7);
  std::optional<int *> source(value.get());
  TypeParam container;
  container.template register_type<scope<dingo::external>,
                                   storage<std::optional<int *> &>>(source);
  EXPECT_EQ(container.template resolve<int *>(), value.get());
  EXPECT_THROW(
      (void)container.template resolve<std::optional<std::shared_ptr<int>>>(),
      dingo::type_not_convertible_exception);
  EXPECT_THROW((void)container.template resolve<std::shared_ptr<int>>(),
               dingo::type_not_convertible_exception);
}

// A value dependency of a shared object is destroyed when the request ends,
// whether the object is constructed or its construction throws.
struct value_consumer {
  explicit value_consumer(counted<1> dependency) : alive(dependency.alive) {}
  bool alive;
};

struct throwing_value_consumer {
  explicit throwing_value_consumer(counted<1> /*dependency*/) {
    throw std::runtime_error("construction failed");
  }
};

TEST(next_runtime_registration,
     a_value_dependency_lives_as_long_as_the_request) {
  counted<1>::clear();
  {
    dingo::container<> container;
    container.register_type<scope<dingo::shared>, storage<value_consumer>>();
    container.register_type<scope<dingo::shared>,
                            storage<throwing_value_consumer>>();
    container.register_type<scope<dingo::unique>, storage<counted<1>>>();

    EXPECT_THROW(container.resolve<throwing_value_consumer &>(),
                 std::runtime_error);
    EXPECT_EQ(counted<1>::live, 0);
    EXPECT_GT(counted<1>::destructions, 0);

    auto &service = container.resolve<value_consumer &>();
    EXPECT_TRUE(service.alive);
    EXPECT_EQ(counted<1>::live, 0);
    EXPECT_EQ(&container.resolve<value_consumer &>(), &service);
    EXPECT_EQ(counted<1>::live, 0);
  }
  EXPECT_EQ(counted<1>::live, 0);
}

// A consumed object is never borrowed: a dependency of an lvalue reference to
// the object of a unique binding is refused, and nothing it made is kept.
struct borrowing_consumer {
  explicit borrowing_consumer(counted<2> &used) : dependency(&used) {}
  counted<2> *dependency;
};

TEST(next_runtime_registration,
     an_lvalue_dependency_on_a_consumed_object_is_refused) {
  counted<2>::clear();
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<borrowing_consumer>>();
  container.register_type<scope<dingo::unique>, storage<counted<2>>>();
  EXPECT_THROW(container.resolve<borrowing_consumer &>(),
               dingo::type_not_convertible_exception);
  EXPECT_EQ(counted<2>::live, 0);
}

// An rvalue dependency of a shared object refers to an object the container
// keeps: it lives as long as the container, longer than the shared object
// that refers to it, and a failed request destroys the one it made.
template <int Id> struct rvalue_consumer {
  explicit rvalue_consumer(counted<Id> &&used) : dependency(&used) {
    if (should_throw) {
      throw std::runtime_error("construction failed");
    }
  }
  ~rvalue_consumer() { dependency_alive_at_destruction = dependency->alive; }
  bool dependency_alive() const { return dependency->alive; }

  counted<Id> *dependency;
  static inline bool should_throw = false;
  static inline bool dependency_alive_at_destruction = false;
};

TEST(next_runtime_registration,
     a_shared_object_does_not_outlive_its_rvalue_dependency) {
  using consumer = rvalue_consumer<3>;
  counted<3>::clear();
  consumer::should_throw = false;
  consumer::dependency_alive_at_destruction = false;
  {
    dingo::container<> container;
    container.register_type<scope<dingo::shared>, storage<consumer>,
                            dingo::dependencies<counted<3> &&>>();
    container.register_type<scope<dingo::unique>, storage<counted<3>>>();
    auto &service = container.resolve<consumer &>();
    EXPECT_TRUE(service.dependency_alive());
    EXPECT_EQ(counted<3>::live, 1);
  }
  EXPECT_TRUE(consumer::dependency_alive_at_destruction);
  EXPECT_EQ(counted<3>::live, 0);
}

TEST(next_runtime_registration,
     a_failed_request_destroys_the_rvalue_dependency_it_made) {
  using consumer = rvalue_consumer<4>;
  counted<4>::clear();
  consumer::should_throw = true;
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<consumer>,
                          dingo::dependencies<counted<4> &&>>();
  container.register_type<scope<dingo::unique>, storage<counted<4>>>();
  EXPECT_THROW(container.resolve<consumer &>(), std::runtime_error);
  EXPECT_EQ(counted<4>::live, 0);

  consumer::should_throw = false;
  auto &service = container.resolve<consumer &>();
  EXPECT_TRUE(service.dependency_alive());
  EXPECT_EQ(counted<4>::live, 1);
}

TEST(next_runtime_registration,
     a_failed_request_keeps_the_rvalue_dependency_of_an_earlier_one) {
  using kept = rvalue_consumer<5>;
  using failing = rvalue_consumer<6>;
  counted<5>::clear();
  counted<6>::clear();
  kept::should_throw = false;
  failing::should_throw = true;
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<kept>,
                          dingo::dependencies<counted<5> &&>>();
  container.register_type<scope<dingo::unique>, storage<counted<5>>>();
  container.register_type<scope<dingo::shared>, storage<failing>,
                          dingo::dependencies<counted<6> &&>>();
  container.register_type<scope<dingo::unique>, storage<counted<6>>>();

  auto &service = container.resolve<kept &>();
  ASSERT_TRUE(service.dependency_alive());
  EXPECT_THROW(container.resolve<failing &>(), std::runtime_error);
  EXPECT_TRUE(service.dependency_alive());
  EXPECT_EQ(&container.resolve<kept &>(), &service);
  EXPECT_EQ(counted<5>::live, 1);
  EXPECT_EQ(counted<6>::live, 0);
  failing::should_throw = false;
}

// The shared object is destroyed first, then the rvalue dependency it refers
// to.
std::vector<int> *destruction_order = nullptr;

struct ordered_dependency {
  ordered_dependency() {}
  ordered_dependency(ordered_dependency &&) noexcept {}
  ~ordered_dependency() {
    alive = false;
    destruction_order->push_back(2);
  }
  bool alive = true;
};

struct ordered_consumer {
  explicit ordered_consumer(ordered_dependency &&dependency)
      : used(&dependency) {}
  ~ordered_consumer() { destruction_order->push_back(used->alive ? 1 : -1); }
  ordered_dependency *used;
};

TEST(next_runtime_registration,
     a_shared_object_is_destroyed_before_its_rvalue_dependency) {
  std::vector<int> events;
  destruction_order = &events;
  {
    dingo::container<> container;
    container.register_type<scope<dingo::shared>, storage<ordered_consumer>,
                            dingo::dependencies<ordered_dependency &&>>();
    container
        .register_type<scope<dingo::unique>, storage<ordered_dependency>>();
    (void)container.resolve<ordered_consumer &>();
    EXPECT_TRUE(events.empty());
  }
  EXPECT_EQ(events, (std::vector<int>{1, 2}));
  destruction_order = nullptr;
}

// A unique object that depends on a shared one, which depends on a unique
// rvalue: the shared object and what it refers to are committed with the
// request of the unique object, and kept.
struct nested_shared {
  explicit nested_shared(counted<7> &&used) : dependency(&used) {}
  counted<7> *dependency;
};

struct outer_unique {
  explicit outer_unique(nested_shared &used) : shared(&used) {}
  nested_shared *shared;
};

TEST(next_runtime_registration,
     a_unique_request_commits_the_shared_objects_it_makes) {
  counted<7>::clear();
  {
    dingo::container<> container;
    container.register_type<scope<dingo::unique>, storage<outer_unique>>();
    container.register_type<scope<dingo::shared>, storage<nested_shared>,
                            dingo::dependencies<counted<7> &&>>();
    container.register_type<scope<dingo::unique>, storage<counted<7>>>();

    outer_unique outer = container.resolve<outer_unique>();
    EXPECT_TRUE(outer.shared->dependency->alive);
    EXPECT_EQ(counted<7>::live, 1);

    auto &shared = container.resolve<nested_shared &>();
    EXPECT_EQ(&shared, outer.shared);
    EXPECT_TRUE(shared.dependency->alive);
    EXPECT_EQ(counted<7>::live, 1);
  }
  EXPECT_EQ(counted<7>::live, 0);
}

// A copy-only type, whose move constructor is deleted, is constructed by
// value and by rvalue reference.
struct copy_only {
  copy_only() : value(31) {}
  copy_only(const copy_only &) = default;
  copy_only(copy_only &&) = delete;
  int value;
};

TEST(next_runtime_registration, constructs_a_copy_only_object) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<copy_only>>();
  copy_only value = container.resolve<copy_only>();
  EXPECT_EQ(value.value, 31);
}

// Finding: the request selects the term of the binding and then throws
// std::logic_error, "the selected term does not complete the request", as the
// object the container keeps is constructed only from an object it could
// move.
TEST(next_runtime_registration,
     DISABLED_refers_to_a_kept_copy_only_object_by_rvalue_reference) {
  dingo::container<> container;
  container.register_type<scope<dingo::unique>, storage<copy_only>>();
  copy_only &&reference = container.resolve<copy_only &&>();
  EXPECT_EQ(reference.value, 31);
}

// A shared object that failed is constructed again by the next request, and a
// failure of a later request keeps the shared objects committed before it.
template <int Id> struct tracked {
  tracked() { ++constructions; }
  ~tracked() { ++destructions; }
  static inline int constructions = 0;
  static inline int destructions = 0;
  static void clear() { constructions = destructions = 0; }
};

struct throwing_dependent {
  explicit throwing_dependent(tracked<1> & /*dependency*/) {
    throw std::runtime_error("construction failed");
  }
};

TEST(next_runtime_registration,
     a_failed_request_keeps_the_shared_objects_committed_before_it) {
  tracked<1>::clear();
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<tracked<1>>>();
  auto &committed = container.resolve<tracked<1> &>();
  container.register_type<scope<dingo::shared>, storage<throwing_dependent>>();
  EXPECT_THROW(container.resolve<throwing_dependent &>(), std::runtime_error);
  EXPECT_EQ(tracked<1>::constructions, 1);
  EXPECT_EQ(tracked<1>::destructions, 0);
  EXPECT_EQ(&container.resolve<tracked<1> &>(), &committed);
}

// The local static binding of a registration constructed during a failed
// request is destroyed with it, and constructed again by the next one.
struct local_consumer {
  explicit local_consumer(tracked<2> & /*dependency*/) {
    if (should_throw) {
      throw std::runtime_error("construction failed");
    }
  }
  static inline bool should_throw = true;
};

TEST(next_runtime_registration,
     a_failed_request_destroys_the_local_objects_it_made) {
  tracked<2>::clear();
  local_consumer::should_throw = true;
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<local_consumer>,
                          dingo::dependencies<tracked<2> &>,
                          dingo::bindings<dingo::bind<scope<dingo::shared>,
                                                      storage<tracked<2>>>>>();
  EXPECT_THROW(container.resolve<local_consumer &>(), std::runtime_error);
  EXPECT_EQ(tracked<2>::constructions, 1);
  EXPECT_EQ(tracked<2>::destructions, 1);

  local_consumer::should_throw = false;
  (void)container.resolve<local_consumer &>();
  EXPECT_EQ(tracked<2>::constructions, 2);
  EXPECT_EQ(tracked<2>::destructions, 1);
}

// A shared_cyclical handle that failed to construct is published again by the
// next request, and a later failure keeps the committed handle.
struct cyclical_iface {
  virtual ~cyclical_iface() = default;
};

struct cyclical_impl : cyclical_iface {
  cyclical_impl() {
    if (should_throw) {
      throw std::runtime_error("construction failed");
    }
  }
  static inline bool should_throw = false;
};

struct cyclical_consumer {
  explicit cyclical_consumer(std::shared_ptr<cyclical_iface> & /*handle*/) {
    throw std::runtime_error("construction failed");
  }
};

TEST(next_runtime_registration,
     a_failed_shared_cyclical_publication_is_published_again) {
  cyclical_impl::should_throw = true;
  dingo::container<> container;
  container.register_type<scope<dingo::shared_cyclical>,
                          storage<std::shared_ptr<cyclical_impl>>,
                          dingo::interfaces<cyclical_iface, cyclical_impl>>();
  EXPECT_THROW(container.resolve<std::shared_ptr<cyclical_iface> &>(),
               std::runtime_error);
  cyclical_impl::should_throw = false;
  auto &resolved = container.resolve<std::shared_ptr<cyclical_iface> &>();
  EXPECT_NE(resolved.get(), nullptr);
}

TEST(next_runtime_registration,
     a_failed_request_keeps_a_committed_shared_cyclical_handle) {
  cyclical_impl::should_throw = false;
  dingo::container<> container;
  container.register_type<scope<dingo::shared_cyclical>,
                          storage<std::shared_ptr<cyclical_impl>>,
                          dingo::interfaces<cyclical_iface, cyclical_impl>>();
  auto &committed = container.resolve<std::shared_ptr<cyclical_iface> &>();
  container
      .register_type<scope<dingo::shared>, storage<cyclical_consumer>,
                     dingo::dependencies<std::shared_ptr<cyclical_iface> &>>();
  EXPECT_THROW(container.resolve<cyclical_consumer &>(), std::runtime_error);
  auto &after_failure = container.resolve<std::shared_ptr<cyclical_iface> &>();
  EXPECT_EQ(&after_failure, &committed);
  EXPECT_EQ(after_failure.get(), committed.get());
}

// The callable factory of a registration is kept until the container is
// destroyed; the one of a registration that is rejected is destroyed when it
// is rejected.
struct factory_lifetime {
  explicit factory_lifetime(int *counter) : destructions(counter) {}
  factory_lifetime(const factory_lifetime &) = delete;
  factory_lifetime(factory_lifetime &&other) noexcept
      : destructions(other.destructions) {
    other.destructions = nullptr;
  }
  ~factory_lifetime() {
    if (destructions != nullptr) {
      ++*destructions;
    }
  }
  int *destructions;
};

struct made_service {};

auto counting_factory(int *destructions) {
  return dingo::callable(
      [state = factory_lifetime(destructions)] { return made_service{}; });
}

TEST(next_runtime_registration,
     keeps_the_factory_of_a_registration_until_the_container_is_destroyed) {
  int destructions = 0;
  {
    dingo::container<> container;
    container.register_type<scope<dingo::shared>, storage<made_service>>(
        counting_factory(&destructions));
    (void)container.resolve<made_service &>();
    EXPECT_EQ(destructions, 0);
  }
  EXPECT_EQ(destructions, 1);
}

TEST(next_runtime_registration,
     destroys_the_factory_of_a_rejected_registration) {
  int first = 0;
  int duplicate = 0;
  {
    dingo::container<> container;
    container.register_type<scope<dingo::shared>, storage<made_service>>(
        counting_factory(&first));
    EXPECT_THROW(
        (container.register_type<scope<dingo::shared>, storage<made_service>>(
            counting_factory(&duplicate))),
        dingo::lookup_already_registered_exception);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(duplicate, 1);
  }
  EXPECT_EQ(first, 1);
  EXPECT_EQ(duplicate, 1);
}

// A registration or a request made while the container serves a request
// belongs to the transaction of the outermost one: it is undone when that one
// fails, and kept when it returns.
struct constructed_service {};
struct registered_service {};

TEST(next_runtime_registration,
     a_failed_construct_undoes_the_registrations_it_made) {
  dingo::container<> container;
  EXPECT_THROW((container.construct<constructed_service>(
                   dingo::callable([&container]() -> constructed_service {
                     container.register_type<scope<dingo::shared>,
                                             storage<registered_service>>();
                     throw std::runtime_error("construction failed");
                   }))),
               std::runtime_error);
  EXPECT_THROW(container.resolve<registered_service &>(),
               dingo::type_not_found_exception);

  container.register_type<scope<dingo::shared>, storage<registered_service>>();
  EXPECT_NO_THROW(container.resolve<registered_service &>());
}

struct resolving_service {
  tracked<3> *dependency;
};

struct failing_resolving_service {
  tracked<4> *dependency;
};

TEST(next_runtime_registration,
     a_nested_request_commits_with_the_outermost_one) {
  tracked<3>::clear();
  {
    dingo::container<> container;
    container.register_type<scope<dingo::shared>, storage<tracked<3>>>();
    container.register_type<scope<dingo::shared>, storage<resolving_service>>(
        dingo::callable([&container] {
          return resolving_service{&container.resolve<tracked<3> &>()};
        }));
    auto &outer = container.resolve<resolving_service &>();
    EXPECT_EQ(outer.dependency, &container.resolve<tracked<3> &>());
    EXPECT_EQ(tracked<3>::constructions, 1);
    EXPECT_EQ(tracked<3>::destructions, 0);
  }
  EXPECT_EQ(tracked<3>::destructions, 1);
}

TEST(next_runtime_registration,
     a_nested_request_is_undone_with_the_outermost_one) {
  tracked<4>::clear();
  {
    dingo::container<> container;
    bool should_throw = true;
    container.register_type<scope<dingo::shared>, storage<tracked<4>>>();
    container.register_type<scope<dingo::shared>,
                            storage<failing_resolving_service>>(
        dingo::callable([&container, &should_throw] {
          auto &dependency = container.resolve<tracked<4> &>();
          if (should_throw) {
            throw std::runtime_error("construction failed");
          }
          return failing_resolving_service{&dependency};
        }));
    EXPECT_THROW(container.resolve<failing_resolving_service &>(),
                 std::runtime_error);
    EXPECT_EQ(tracked<4>::constructions, 1);
    EXPECT_EQ(tracked<4>::destructions, 1);

    should_throw = false;
    auto &outer = container.resolve<failing_resolving_service &>();
    EXPECT_EQ(outer.dependency, &container.resolve<tracked<4> &>());
    EXPECT_EQ(tracked<4>::constructions, 2);
    EXPECT_EQ(tracked<4>::destructions, 1);
  }
  EXPECT_EQ(tracked<4>::destructions, 2);
}

struct retried_service {};
struct registered_dependency {};

TEST(next_runtime_registration,
     a_registration_of_a_failed_factory_is_undone_so_a_retry_registers_it) {
  bool should_throw = true;
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<retried_service>>(
      dingo::callable([&container, &should_throw] {
        container.register_type<scope<dingo::shared>,
                                storage<registered_dependency>>();
        if (should_throw) {
          throw std::runtime_error("construction failed");
        }
        return retried_service{};
      }));
  EXPECT_THROW(container.resolve<retried_service &>(), std::runtime_error);
  should_throw = false;
  EXPECT_NO_THROW(container.resolve<retried_service &>());
  EXPECT_NO_THROW(container.resolve<registered_dependency &>());
}

// The local bindings of a unique registration, made by a function factory,
// serve every object the registration constructs.
struct local_unique_dependency {
  explicit local_unique_dependency(int number) : value(number) { ++live; }
  local_unique_dependency(const local_unique_dependency &other)
      : value(other.value) {
    ++live;
  }
  local_unique_dependency(local_unique_dependency &&other) noexcept
      : value(other.value) {
    ++live;
  }
  ~local_unique_dependency() { --live; }
  int value;
  static inline int live = 0;
};

local_unique_dependency make_local_unique_dependency() {
  return local_unique_dependency(13);
}

struct local_unique_service {
  explicit local_unique_service(local_unique_dependency dependency)
      : value(dependency.value),
        dependency_was_live(local_unique_dependency::live != 0) {}
  int value;
  bool dependency_was_live;
};

TEST(next_runtime_registration,
     a_unique_registration_resolves_from_its_local_bindings) {
  local_unique_dependency::live = 0;
  dingo::container<> container;
  container.register_type<
      scope<dingo::unique>, storage<local_unique_service>,
      dingo::dependencies<local_unique_dependency>,
      dingo::bindings<dingo::bind<
          scope<dingo::unique>, storage<local_unique_dependency>,
          dingo::factory<dingo::function<&make_local_unique_dependency>>>>>();
  auto first = container.resolve<local_unique_service>();
  auto second = container.resolve<local_unique_service>();
  EXPECT_EQ(first.value + second.value, 26);
  EXPECT_TRUE(first.dependency_was_live);
  EXPECT_EQ(local_unique_dependency::live, 0);
  EXPECT_THROW(container.resolve<local_unique_dependency>(),
               dingo::type_not_found_exception);
}

// In a container whose every domain holds many bindings, the local bindings
// of two registrations stay isolated, for a singular and for a collection
// request.
struct domain_dependency {
  explicit domain_dependency(int number = 0) : value(number) {}
  int value;
};

struct many_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::base<dingo::many, dingo::ordered>>;
};

struct singular_service {
  explicit singular_service(domain_dependency &dependency)
      : value(dependency.value) {}
  int value;
};

struct collecting_service {
  explicit collecting_service(std::vector<domain_dependency *> dependencies)
      : value(dependencies.front()->value), count(dependencies.size()) {}
  int value;
  std::size_t count;
};

TEST(next_runtime_registration,
     isolates_local_bindings_in_a_container_of_many) {
  dingo::container<many_traits> container;
  auto first =
      container.register_type<scope<dingo::shared>, storage<singular_service>,
                              dingo::dependencies<domain_dependency &>>();
  auto second = container.register_type<
      scope<dingo::shared>, storage<collecting_service>,
      dingo::dependencies<std::vector<domain_dependency *>>>();
  first->register_type<scope<dingo::shared>, storage<domain_dependency>>(
      dingo::callable([] { return domain_dependency{11}; }));
  second->register_type<scope<dingo::shared>, storage<domain_dependency>>(
      dingo::callable([] { return domain_dependency{22}; }));
  EXPECT_EQ(container.resolve<singular_service &>().value, 11);
  auto &collecting = container.resolve<collecting_service &>();
  EXPECT_EQ(collecting.value, 22);
  EXPECT_EQ(collecting.count, 1u);
}

// The bindings registered through the proxies of nested registrations are
// kept when a request through them fails, and serve the next request.
bool proxy_service_should_throw = false;

struct proxy_static_dependency {
  proxy_static_dependency() : value(7) {}
  int value;
};

struct proxy_runtime_dependency {
  proxy_runtime_dependency() : value(11) {}
  int value;
};

struct proxy_nested_dependency {
  proxy_nested_dependency() : value(13) {}
  int value;
};

struct proxy_service {
  proxy_service(proxy_static_dependency &static_value,
                proxy_runtime_dependency &runtime_value,
                proxy_nested_dependency &nested_value)
      : value(static_value.value + runtime_value.value + nested_value.value) {
    if (proxy_service_should_throw) {
      throw std::runtime_error("construction failed");
    }
  }
  int value;
};

struct proxy_root {
  explicit proxy_root(proxy_service &dependency) : value(dependency.value) {}
  int value;
};

TEST(next_runtime_registration,
     keeps_the_local_bindings_of_proxies_when_a_request_fails) {
  dingo::container<> container;
  proxy_service_should_throw = true;
  auto child = container.register_type<
      scope<dingo::shared>, storage<proxy_root>,
      dingo::dependencies<proxy_service &>,
      dingo::bindings<dingo::bind<scope<dingo::shared>,
                                  storage<proxy_static_dependency>>>>();
  child->register_type<scope<dingo::shared>,
                       storage<proxy_runtime_dependency>>();
  auto nested = child->register_type<
      scope<dingo::shared>, storage<proxy_service>,
      dingo::dependencies<proxy_static_dependency &, proxy_runtime_dependency &,
                          proxy_nested_dependency &>>();
  nested
      ->register_type<scope<dingo::shared>, storage<proxy_nested_dependency>>();

  EXPECT_THROW(container.resolve<proxy_root &>(), std::runtime_error);
  proxy_service_should_throw = false;
  EXPECT_EQ(container.resolve<proxy_root &>().value, 31);
}

// The local bindings of a registration follow the lookup declarations of the
// container, here a dense array of one key value.
struct array_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, domain_dependency,
                                        dingo::one, dingo::array<1>>>;
};

using key_zero = dingo::key_type<std::size_t, 0>;

template <int Id> struct keyed_service {
  explicit keyed_service(dingo::dependency<domain_dependency &, key_zero> used)
      : value(used) {}
  domain_dependency &value;
};

TEST(next_runtime_registration,
     local_bindings_follow_the_dense_array_of_the_container) {
  dingo::container<array_traits> container;
  auto first = container.register_type<
      scope<dingo::shared>, storage<keyed_service<1>>,
      dingo::dependencies<dingo::dependency<domain_dependency &, key_zero>>,
      dingo::bindings<dingo::bind<scope<dingo::shared>,
                                  storage<proxy_static_dependency>>>>();
  auto second = container.register_type<
      scope<dingo::shared>, storage<keyed_service<2>>,
      dingo::dependencies<dingo::dependency<domain_dependency &, key_zero>>>();
  first->register_type<scope<dingo::shared>, storage<domain_dependency>>(
      dingo::callable([] { return domain_dependency{11}; }),
      dingo::key_value{std::size_t(0)});
  second->register_type<scope<dingo::shared>, storage<domain_dependency>>(
      dingo::callable([] { return domain_dependency{22}; }),
      dingo::key_value{std::size_t(0)});
  EXPECT_THROW(
      (first->register_type<scope<dingo::shared>, storage<domain_dependency>>(
          dingo::callable([] { return domain_dependency{33}; }),
          dingo::key_value{std::size_t(1)})),
      dingo::type_index_out_of_range_exception);
  EXPECT_EQ(container.resolve<keyed_service<1> &>().value.value, 11);
  EXPECT_EQ(container.resolve<keyed_service<2> &>().value.value, 22);
}

// A keyed collection of a registration with local bindings of the key holds
// the local and the container's bindings; one without local bindings of its
// key holds the container's.
struct associative_dependency {
  explicit associative_dependency(int number = 0) : value(number) {}
  int value;
};

struct typed_dependency {
  explicit typed_dependency(int number = 0) : value(number) {}
  int value;
};

struct typed_key {};

struct collection_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, associative_dependency,
                                        dingo::many, dingo::ordered>,
                     dingo::typed<typed_key, typed_dependency, dingo::many>>;
};

struct keyed_collection_service {
  keyed_collection_service(
      dingo::dependency<std::vector<associative_dependency *>, key_zero>
          associative,
      dingo::dependency<std::vector<typed_dependency *>,
                        dingo::key_type<typed_key>>
          typed)
      : associative_values(associative), typed_values(typed) {}
  std::vector<associative_dependency *> associative_values;
  std::vector<typed_dependency *> typed_values;
};

TEST(next_runtime_registration,
     keyed_collections_hold_the_local_and_the_container_bindings) {
  dingo::container<collection_traits> container;
  container
      .register_type<scope<dingo::shared>, storage<associative_dependency>>(
          dingo::callable([] { return associative_dependency{11}; }),
          dingo::key_value{std::size_t(0)});
  container.register_type<scope<dingo::shared>, storage<typed_dependency>,
                          dingo::key_type<typed_key>>(
      dingo::callable([] { return typed_dependency{33}; }));
  auto child = container.register_type<
      scope<dingo::shared>, storage<keyed_collection_service>,
      dingo::dependencies<
          dingo::dependency<std::vector<associative_dependency *>, key_zero>,
          dingo::dependency<std::vector<typed_dependency *>,
                            dingo::key_type<typed_key>>>>();
  child->register_type<scope<dingo::shared>, storage<associative_dependency>>(
      dingo::callable([] { return associative_dependency{22}; }),
      dingo::key_value{std::size_t(0)});

  auto &service = container.resolve<keyed_collection_service &>();
  std::vector<int> associative;
  for (auto *element : service.associative_values) {
    associative.push_back(element->value);
  }
  EXPECT_EQ(associative, (std::vector<int>{11, 22}));
  ASSERT_EQ(service.typed_values.size(), 1u);
  EXPECT_EQ(service.typed_values.front()->value, 33);
}

// In a domain of one, a local runtime binding of a key a local static binding
// of the registration holds is rejected; a binding of the container is of
// another level, so a collection holds it and the local one.
domain_dependency make_local_static_dependency() {
  return domain_dependency{22};
}

TEST(next_runtime_registration,
     rejects_a_local_runtime_binding_of_a_key_a_local_static_one_holds) {
  dingo::container<> container;
  container.register_type<scope<dingo::shared>, storage<domain_dependency>>(
      dingo::callable([] { return domain_dependency{11}; }));
  auto child = container.register_type<
      scope<dingo::shared>, storage<collecting_service>,
      dingo::dependencies<std::vector<domain_dependency *>>,
      dingo::bindings<dingo::bind<
          scope<dingo::shared>, storage<domain_dependency>,
          dingo::factory<dingo::function<&make_local_static_dependency>>>>>();
  EXPECT_THROW(
      (child->register_type<scope<dingo::shared>, storage<domain_dependency>>(
          dingo::callable([] { return domain_dependency{33}; }))),
      dingo::lookup_already_registered_exception);
  auto &service = container.resolve<collecting_service &>();
  EXPECT_EQ(service.value, 11);
  EXPECT_EQ(service.count, 2u);
}

} // namespace
