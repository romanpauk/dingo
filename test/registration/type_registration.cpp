//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/core/binding_model.h>
#include <dingo/core/factory_traits.h>
#include <dingo/factory/callable.h>
#include <dingo/registration/constructor.h>
#include <dingo/registration/type_registration.h>
#include <dingo/resolution/conversion_cache.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/rtti/rtti.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/transaction.h>
#include <dingo/storage/external.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>
#include <dingo/storage/unique.h>
#include <dingo/type/rebind_type.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

using namespace dingo;

namespace {
int build_from_double_and_cstr(double, const char *) { return 0; }

struct runtime_binding_first_interface {
  virtual ~runtime_binding_first_interface() = default;
};

struct runtime_binding_second_interface {
  virtual ~runtime_binding_second_interface() = default;
};

struct runtime_binding_shared_implementation
    : runtime_binding_first_interface,
      runtime_binding_second_interface {};

struct runtime_binding_local_dependency {
  explicit runtime_binding_local_dependency(int init_value)
      : value(init_value) {}

  int value;
};

runtime_binding_local_dependency make_runtime_binding_local_dependency() {
  return runtime_binding_local_dependency{7};
}

struct runtime_binding_local_service {
  explicit runtime_binding_local_service(
      runtime_binding_local_dependency &dependency)
      : value(dependency.value) {}

  int value;
};

struct counting_runtime_binding_storage {};

struct counting_runtime_binding_container {
  using allocator_type = std::allocator<char>;
  using parent_container_type = counting_runtime_binding_container;

  explicit counting_runtime_binding_container(
      counting_runtime_binding_container *,
      allocator_type allocator = allocator_type())
      : allocator_(allocator) {
    ++constructions;
  }

  allocator_type &get_allocator() { return allocator_; }

  static int constructions;

private:
  allocator_type allocator_;
};

int counting_runtime_binding_container::constructions = 0;

struct runtime_arena_tracker {
  explicit runtime_arena_tracker(std::vector<int> *events, int value)
      : events_(events), value_(value) {}

  ~runtime_arena_tracker() { events_->push_back(value_); }

  std::vector<int> *events_;
  int value_;
};

struct runtime_registration_lifetime_service {
  ~runtime_registration_lifetime_service() {}
};

struct runtime_registration_lifetime_factory {
  explicit runtime_registration_lifetime_factory(int *destructions)
      : destructions_(destructions) {}

  runtime_registration_lifetime_factory(
      const runtime_registration_lifetime_factory &) = default;

  runtime_registration_lifetime_factory(
      runtime_registration_lifetime_factory &&other) noexcept
      : destructions_(other.destructions_) {
    other.destructions_ = nullptr;
  }

  ~runtime_registration_lifetime_factory() {
    if (destructions_) {
      ++*destructions_;
    }
  }

  template <typename Type, typename Context, typename Container>
  Type construct(Context &, Container &) const {
    return Type{};
  }

  template <typename Type, typename Context, typename Container>
  void construct(void *ptr, Context &, Container &) const {
    using object_type = std::remove_pointer_t<Type>;
    new (ptr) object_type();
  }

  int *destructions_;
};

struct runtime_unique_local_dependency {
  explicit runtime_unique_local_dependency(int init_value) : value(init_value) {
    ++live_count;
  }

  runtime_unique_local_dependency(const runtime_unique_local_dependency &other)
      : runtime_unique_local_dependency(other.value) {}

  runtime_unique_local_dependency(runtime_unique_local_dependency &&other)
      : runtime_unique_local_dependency(other.value) {}

  ~runtime_unique_local_dependency() { --live_count; }

  int value;
  static int live_count;
};

int runtime_unique_local_dependency::live_count = 0;

runtime_unique_local_dependency make_runtime_unique_local_dependency() {
  return runtime_unique_local_dependency{13};
}

struct runtime_unique_local_service {
  explicit runtime_unique_local_service(
      runtime_unique_local_dependency &dependency)
      : value(dependency.value),
        dependency_was_live(runtime_unique_local_dependency::live_count != 0) {}

  int value;
  bool dependency_was_live;
};

struct runtime_scoped_source_dependency {
  runtime_scoped_source_dependency() = default;
  runtime_scoped_source_dependency(const runtime_scoped_source_dependency &) {}
  ~runtime_scoped_source_dependency() { ++destructions; }

  static int destructions;
};

int runtime_scoped_source_dependency::destructions = 0;

struct runtime_scoped_source_throwing_service {
  explicit runtime_scoped_source_throwing_service(
      runtime_scoped_source_dependency) {
    throw std::runtime_error("resolution failed");
  }
};

struct runtime_scoped_local_dependency {
  runtime_scoped_local_dependency() { ++constructions; }
  ~runtime_scoped_local_dependency() { ++destructions; }

  static int constructions;
  static int destructions;
};

int runtime_scoped_local_dependency::constructions = 0;
int runtime_scoped_local_dependency::destructions = 0;

struct runtime_scoped_local_service {
  explicit runtime_scoped_local_service(runtime_scoped_local_dependency &) {
    if (should_throw) {
      throw std::runtime_error("resolution failed");
    }
  }

  static bool should_throw;
};

bool runtime_scoped_local_service::should_throw = true;

struct runtime_retained_source_unique_dependency {
  runtime_retained_source_unique_dependency() { ++live_count; }
  runtime_retained_source_unique_dependency(
      const runtime_retained_source_unique_dependency &)
      : runtime_retained_source_unique_dependency() {}
  runtime_retained_source_unique_dependency(
      runtime_retained_source_unique_dependency &&)
      : runtime_retained_source_unique_dependency() {}

  ~runtime_retained_source_unique_dependency() {
    alive = false;
    ++destructions;
    --live_count;
  }

  bool alive = true;
  static int live_count;
  static int destructions;
};

int runtime_retained_source_unique_dependency::live_count = 0;
int runtime_retained_source_unique_dependency::destructions = 0;

struct runtime_retained_source_shared_service {
  explicit runtime_retained_source_shared_service(
      runtime_retained_source_unique_dependency &dependency)
      : dependency_(std::addressof(dependency)) {
    if (should_throw) {
      throw std::runtime_error("shared construction failed");
    }
  }

  ~runtime_retained_source_shared_service() {
    dependency_was_live_at_destruction = dependency_->alive;
  }

  bool dependency_is_live() const { return dependency_->alive; }

  runtime_retained_source_unique_dependency *dependency_;
  static bool should_throw;
  static bool dependency_was_live_at_destruction;
};

bool runtime_retained_source_shared_service::should_throw = false;
bool
    runtime_retained_source_shared_service::dependency_was_live_at_destruction =
        false;

struct runtime_retained_source_pointer_dependency {
  runtime_retained_source_pointer_dependency() { ++live_count; }

  ~runtime_retained_source_pointer_dependency() {
    alive = false;
    --live_count;
  }

  bool alive = true;
  static int live_count;
};

int runtime_retained_source_pointer_dependency::live_count = 0;

struct runtime_retained_source_pointer_factory {
  template <typename Type, typename Context, typename Container>
  Type construct(Context &context, Container &) const {
    static_assert(std::is_pointer_v<Type>);
    using object_type = std::remove_pointer_t<Type>;
    return std::addressof(context.template construct<object_type>());
  }

  template <typename Type, typename Context, typename Container>
  void construct(void *ptr, Context &context, Container &container) const {
    new (ptr) Type(construct<Type>(context, container));
  }
};

struct runtime_retained_source_pointer_service {
  explicit runtime_retained_source_pointer_service(
      runtime_retained_source_pointer_dependency *dependency)
      : dependency_(dependency) {}

  ~runtime_retained_source_pointer_service() {
    dependency_was_live_at_destruction = dependency_->alive;
  }

  bool dependency_is_live() const { return dependency_->alive; }

  runtime_retained_source_pointer_dependency *dependency_;
  static bool dependency_was_live_at_destruction;
};

bool runtime_retained_source_pointer_service::
    dependency_was_live_at_destruction = false;

struct runtime_retained_source_order_dependency {
  runtime_retained_source_order_dependency() = default;
  runtime_retained_source_order_dependency(
      const runtime_retained_source_order_dependency &) {}
  runtime_retained_source_order_dependency(
      runtime_retained_source_order_dependency &&) {}

  ~runtime_retained_source_order_dependency() {
    alive = false;
    events->push_back(2);
  }

  bool alive = true;
  static std::vector<int> *events;
};

std::vector<int> *runtime_retained_source_order_dependency::events = nullptr;

struct runtime_retained_source_order_service {
  explicit runtime_retained_source_order_service(
      runtime_retained_source_order_dependency &dependency)
      : dependency_(std::addressof(dependency)) {}

  ~runtime_retained_source_order_service() {
    runtime_retained_source_order_dependency::events->push_back(
        dependency_->alive ? 1 : -1);
  }

  bool dependency_is_live() const { return dependency_->alive; }

  runtime_retained_source_order_dependency *dependency_;
};

struct runtime_policy_nested_unique_dependency {
  runtime_policy_nested_unique_dependency() { ++live_count; }
  runtime_policy_nested_unique_dependency(
      const runtime_policy_nested_unique_dependency &)
      : runtime_policy_nested_unique_dependency() {}
  runtime_policy_nested_unique_dependency(
      runtime_policy_nested_unique_dependency &&)
      : runtime_policy_nested_unique_dependency() {}

  ~runtime_policy_nested_unique_dependency() {
    alive = false;
    --live_count;
  }

  bool alive = true;
  static int live_count;
};

int runtime_policy_nested_unique_dependency::live_count = 0;

struct runtime_policy_nested_shared_service {
  explicit runtime_policy_nested_shared_service(
      runtime_policy_nested_unique_dependency &dependency)
      : dependency_(std::addressof(dependency)) {}

  bool dependency_is_live() const { return dependency_->alive; }

  runtime_policy_nested_unique_dependency *dependency_;
};

struct runtime_policy_outer_unique_service {
  explicit runtime_policy_outer_unique_service(
      runtime_policy_nested_shared_service &shared)
      : shared_(std::addressof(shared)) {}

  bool shared_dependency_is_live() const {
    return shared_->dependency_is_live();
  }

  runtime_policy_nested_shared_service *shared_;
};

struct runtime_policy_value_unique_dependency {
  runtime_policy_value_unique_dependency() { ++live_count; }
  runtime_policy_value_unique_dependency(
      const runtime_policy_value_unique_dependency &other)
      : value(other.value) {
    ++live_count;
  }
  runtime_policy_value_unique_dependency(
      runtime_policy_value_unique_dependency &&other) noexcept
      : value(other.value) {
    ++live_count;
  }

  ~runtime_policy_value_unique_dependency() {
    ++destructions;
    --live_count;
  }

  int value = 29;
  static int live_count;
  static int destructions;
};

int runtime_policy_value_unique_dependency::live_count = 0;
int runtime_policy_value_unique_dependency::destructions = 0;

struct runtime_policy_value_shared_service {
  explicit runtime_policy_value_shared_service(
      runtime_policy_value_unique_dependency dependency)
      : value(dependency.value) {}

  int value;
};

struct runtime_policy_detected_tracker {
  runtime_policy_detected_tracker() : value(next_value) {}
  runtime_policy_detected_tracker(const runtime_policy_detected_tracker &)
      : runtime_policy_detected_tracker() {}
  runtime_policy_detected_tracker(runtime_policy_detected_tracker &&)
      : runtime_policy_detected_tracker() {}

  ~runtime_policy_detected_tracker() { events->push_back(value); }

  int value;
  static int next_value;
  static std::vector<int> *events;
};

int runtime_policy_detected_tracker::next_value = 0;
std::vector<int> *runtime_policy_detected_tracker::events = nullptr;

struct nested_transaction_outer_service {};
struct nested_transaction_registered_service {};

struct nested_transaction_resolved_service {
  nested_transaction_resolved_service() { ++constructions; }
  ~nested_transaction_resolved_service() { ++destructions; }

  static int constructions;
  static int destructions;
};

int nested_transaction_resolved_service::constructions = 0;
int nested_transaction_resolved_service::destructions = 0;

struct nested_transaction_resolving_service {
  nested_transaction_resolved_service *dependency;
};

struct runtime_shared_state_dependency {
  runtime_shared_state_dependency() { ++constructions; }
  ~runtime_shared_state_dependency() { ++destructions; }

  static int constructions;
  static int destructions;
};

int runtime_shared_state_dependency::constructions = 0;
int runtime_shared_state_dependency::destructions = 0;

struct runtime_shared_state_throwing_consumer {
  explicit runtime_shared_state_throwing_consumer(
      runtime_shared_state_dependency &) {
    throw std::runtime_error("dependent resolution failed");
  }
};

struct runtime_shared_retry_service {
  runtime_shared_retry_service() {
    ++construction_attempts;
    if (should_throw) {
      throw std::runtime_error("shared construction failed");
    }
  }

  static int construction_attempts;
  static bool should_throw;
};

int runtime_shared_retry_service::construction_attempts = 0;
bool runtime_shared_retry_service::should_throw = true;

struct runtime_cyclical_rollback_interface {
  virtual ~runtime_cyclical_rollback_interface() = default;
};

struct runtime_cyclical_rollback_implementation
    : runtime_cyclical_rollback_interface {
  runtime_cyclical_rollback_implementation() {
    if (should_throw) {
      throw std::runtime_error("cyclical construction failed");
    }
  }

  static bool should_throw;
};

bool runtime_cyclical_rollback_implementation::should_throw = true;

struct runtime_cyclical_rollback_consumer {
  explicit runtime_cyclical_rollback_consumer(
      std::shared_ptr<runtime_cyclical_rollback_interface> &) {
    throw std::runtime_error("dependent resolution failed");
  }
};
} // namespace

TEST(type_registration_test, get_type) {
  struct A {};
  using list = type_list<storage<A>, factory<A>, interfaces<A, A>>;
  using storage_type = detail::get_type_t<storage<void>, list>;
  static_assert(std::is_same_v<storage_type, storage<A>>);

  using factory_type = detail::get_type_t<factory<void>, list>;
  static_assert(std::is_same_v<factory_type, factory<A>>);

  using interface_type = detail::get_type_t<interfaces<void>, list>;
  static_assert(std::is_same_v<interface_type, interfaces<A, A>>);
}

TEST(type_registration_test, registration_basic) {
  using registration =
      type_registration<storage<int>, interfaces<double, float>, factory<int>,
                        scope<int>>;

  static_assert(
      std::is_same_v<typename registration::storage_type, storage<int>>);
  static_assert(std::is_same_v<typename registration::interface_type,
                               interfaces<double, float>>);
  static_assert(
      std::is_same_v<typename registration::factory_type, factory<int>>);
  static_assert(std::is_same_v<typename registration::scope_type, scope<int>>);
}

TEST(type_registration_test, registration_prefers_first_explicit_match) {
  using registration =
      type_registration<scope<int>, scope<float>, storage<double>,
                        storage<char>, factory<long>, factory<short>,
                        interfaces<unsigned>, interfaces<bool>,
                        conversions<void *>, conversions<int *>,
                        dependencies<long>, dependencies<short>>;

  static_assert(std::is_same_v<typename registration::scope_type, scope<int>>);
  static_assert(
      std::is_same_v<typename registration::storage_type, storage<double>>);
  static_assert(
      std::is_same_v<typename registration::factory_type, factory<long>>);
  static_assert(std::is_same_v<typename registration::interface_type,
                               interfaces<unsigned>>);
  static_assert(std::is_same_v<typename registration::conversions_type,
                               conversions<void *>>);
  static_assert(std::is_same_v<typename registration::dependencies_type,
                               dependencies<long>>);
}

TEST(type_registration_test, registration_deduction) {
  static_assert(
      std::is_same_v<
          typename type_registration<scope<int>, factory<int>>::storage_type,
          storage<int>>);
  static_assert(
      std::is_same_v<
          typename type_registration<scope<int>, factory<int>>::interface_type,
          interfaces<int>>);

  static_assert(
      std::is_same_v<
          typename type_registration<scope<int>, storage<int>>::interface_type,
          interfaces<int>>);
  static_assert(
      std::is_same_v<
          typename type_registration<scope<int>, storage<int>>::factory_type,
          factory<constructor<int>>>);
}

TEST(type_registration_test, registration_default_interface_and_conversions) {
  using registration_from_factory =
      type_registration<scope<shared>, factory<int>>;
  static_assert(std::is_same_v<typename registration_from_factory::storage_type,
                               storage<int>>);
  static_assert(
      std::is_same_v<typename registration_from_factory::interface_type,
                     interfaces<int>>);
  static_assert(std::is_same_v<
                typename registration_from_factory::conversions_type,
                conversions<detail::conversions<shared, int, runtime_type>>>);

  using registration_from_storage =
      type_registration<scope<shared>, storage<int>>;
  static_assert(
      std::is_same_v<typename registration_from_storage::interface_type,
                     interfaces<int>>);
  static_assert(std::is_same_v<
                typename registration_from_storage::conversions_type,
                conversions<detail::conversions<shared, int, runtime_type>>>);
}

TEST(type_registration_test,
     registration_dependencies_default_from_factory_traits) {
  struct service {
    explicit service(int, float) {}
  };
  struct selected_service {
    DINGO_CONSTRUCTOR(selected_service(int, float)) {}
  };

  using ctor_registration =
      type_registration<scope<unique>, storage<service>,
                        factory<constructor<service(int, float)>>>;
  static_assert(std::is_same_v<typename ctor_registration::dependencies_type,
                               dependencies<int, float>>);

  using detected_registration =
      type_registration<scope<unique>, storage<selected_service>>;
  static_assert(
      std::is_same_v<typename detected_registration::dependencies_type,
                     dependencies<int, float>>);

  using function_registration =
      type_registration<scope<unique>, storage<int>,
                        factory<function<build_from_double_and_cstr>>>;
  static_assert(
      std::is_same_v<typename function_registration::dependencies_type,
                     dependencies<double, const char *>>);
}

TEST(type_registration_test,
     registration_dependencies_prefer_explicit_metadata) {
  struct service {
    explicit service(int) {}
  };

  using registration = type_registration<scope<unique>, storage<service>,
                                         factory<constructor<service(int)>>,
                                         dependencies<float, bool>>;
  static_assert(std::is_same_v<typename registration::dependencies_type,
                               dependencies<float, bool>>);
}

TEST(type_registration_test,
     registration_explicit_dependencies_shape_default_constructor_factory) {
  struct service {
    service(int, float) {}
    service(double, bool) {}
  };

  using registration = type_registration<scope<unique>, storage<service>,
                                         dependencies<double, bool>>;

  static_assert(std::is_same_v<typename registration::factory_type,
                               factory<constructor<service(double, bool)>>>);
  static_assert(std::is_same_v<typename registration::dependencies_type,
                               dependencies<double, bool>>);
}

TEST(type_registration_test, factory_traits_report_dependencies) {
  struct service {
    explicit service(int, float) {}
  };
  struct selected_service {
    DINGO_CONSTRUCTOR(selected_service(int, float)) {}
  };
  struct defaulted_service {};

  using constructor_factory = constructor<service(int, float)>;
  static_assert(
      std::is_same_v<typename factory_traits<constructor_factory>::dependencies,
                     type_list<int, float>>);
  static_assert(factory_traits<constructor_factory>::has_explicit_dependencies);
  static_assert(factory_traits<constructor_factory>::is_compile_time_bindable);

  using detected_factory = constructor<selected_service>;
  static_assert(
      std::is_same_v<typename factory_traits<detected_factory>::dependencies,
                     type_list<int, float>>);
  static_assert(!factory_traits<detected_factory>::has_explicit_dependencies);
  static_assert(factory_traits<detected_factory>::is_compile_time_bindable);

  using defaulted_factory = constructor<defaulted_service>;
  static_assert(
      std::is_same_v<typename factory_traits<defaulted_factory>::dependencies,
                     type_list<>>);
  static_assert(!factory_traits<defaulted_factory>::has_explicit_dependencies);
  static_assert(factory_traits<defaulted_factory>::is_compile_time_bindable);

  using function_factory = function<build_from_double_and_cstr>;
  static_assert(
      std::is_same_v<typename factory_traits<function_factory>::dependencies,
                     type_list<double, const char *>>);
  static_assert(factory_traits<function_factory>::has_explicit_dependencies);
  static_assert(factory_traits<function_factory>::is_compile_time_bindable);

  auto callable_factory =
      callable<int(short, long)>([](short, long) { return 0; });
  using callable_factory_type = decltype(callable_factory);
  static_assert(std::is_same_v<
                typename factory_traits<callable_factory_type>::dependencies,
                type_list<short, long>>);
  static_assert(
      factory_traits<callable_factory_type>::has_explicit_dependencies);
  static_assert(
      !factory_traits<callable_factory_type>::is_compile_time_bindable);
}

TEST(type_registration_test, registration_specialization) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};
  struct test_container {
    using rtti_type = rtti<typeid_provider>;
    using allocator_type = std::allocator<char>;

    explicit test_container(test_container *,
                            allocator_type allocator = allocator_type())
        : allocator_(allocator) {}

    allocator_type &get_allocator() { return allocator_; }

  private:
    allocator_type allocator_;
  };

  struct test_state_owner {
    using allocator_type = test_container::allocator_type;
    using parent_container_type = test_container;

    explicit test_state_owner(test_container *parent)
        : allocator_(parent->get_allocator()), runtime_(allocator_),
          parent_(parent) {}

    container_runtime<allocator_type> &runtime() { return runtime_; }
    parent_container_type *parent() { return parent_; }
    allocator_type &get_allocator() { return allocator_; }

  private:
    allocator_type allocator_;
    container_runtime<allocator_type> runtime_;
    parent_container_type *parent_;
  };

  using shared_registration = type_registration<scope<shared>, storage<A>>;
  using shared_storage = detail::storage<
      typename shared_registration::scope_type::type,
      typename shared_registration::storage_type::type,
      rebind_type_t<
          typename shared_registration::storage_type::type,
          normalized_type_t<typename shared_registration::storage_type::type>>,
      typename shared_registration::factory_type::type,
      typename shared_registration::conversions_type::type>;
  using shared_binding_data =
      runtime_binding_state<test_state_owner, test_container, shared_storage>;
  using shared_binding =
      runtime_binding<test_container, A, shared_storage, shared_binding_data>;

  using shared_ptr_registration =
      type_registration<scope<shared>, storage<std::shared_ptr<A>>>;
  using shared_ptr_storage = detail::storage<
      typename shared_ptr_registration::scope_type::type,
      typename shared_ptr_registration::storage_type::type,
      rebind_type_t<typename shared_ptr_registration::storage_type::type,
                    normalized_type_t<
                        typename shared_ptr_registration::storage_type::type>>,
      typename shared_ptr_registration::factory_type::type,
      typename shared_ptr_registration::conversions_type::type>;
  using shared_ptr_binding_data =
      runtime_binding_state<test_state_owner, test_container,
                            shared_ptr_storage>;
  using shared_ptr_binding =
      runtime_binding<test_container, A, shared_ptr_storage,
                      shared_ptr_binding_data>;

  using external_registration =
      type_registration<scope<external>, storage<A &>>;
  using external_storage = detail::storage<
      typename external_registration::scope_type::type,
      typename external_registration::storage_type::type,
      rebind_type_t<typename external_registration::storage_type::type,
                    normalized_type_t<
                        typename external_registration::storage_type::type>>,
      typename external_registration::factory_type::type,
      typename external_registration::conversions_type::type>;
  using external_binding_data =
      runtime_binding_state<test_state_owner, test_container, external_storage>;
  using external_binding = runtime_binding<test_container, A, external_storage,
                                           external_binding_data>;

  using external_shared_registration =
      type_registration<scope<external>, storage<std::shared_ptr<A>>>;
  using external_shared_storage = detail::storage<
      typename external_shared_registration::scope_type::type,
      typename external_shared_registration::storage_type::type,
      rebind_type_t<typename external_shared_registration::storage_type::type,
                    normalized_type_t<typename external_shared_registration::
                                          storage_type::type>>,
      typename external_shared_registration::factory_type::type,
      typename external_shared_registration::conversions_type::type>;
  using external_shared_binding_data =
      runtime_binding_state<test_state_owner, test_container,
                            external_shared_storage>;
  using external_shared_binding =
      runtime_binding<test_container, A, external_shared_storage,
                      external_shared_binding_data>;

  using nested_registration =
      type_registration<scope<shared>,
                        storage<std::shared_ptr<std::unique_ptr<A>>>,
                        interfaces<I>>;
  using nested_storage_type = typename nested_registration::storage_type::type;
  using nested_interface_type =
      type_list_head_t<typename nested_registration::interface_type::type>;
  using nested_stored_type = rebind_leaf_t<
      nested_storage_type,
      std::conditional_t<std::has_virtual_destructor_v<nested_interface_type> &&
                             is_interface_storage_rebindable_v<
                                 nested_storage_type, nested_interface_type>,
                         nested_interface_type,
                         leaf_type_t<nested_storage_type>>>;

  // Nested wrapper requests can still borrow to the leaf interface, but
  // built-in smart handles only rebind when C++ itself has the direct handle
  // conversion.
  static_assert(is_interface_storage_rebindable_v<std::shared_ptr<A>, I>);
  static_assert(is_interface_storage_rebindable_v<std::unique_ptr<A>, I>);
  static_assert(
      !is_interface_storage_rebindable_v<std::shared_ptr<std::unique_ptr<A>>,
                                         I>);
  static_assert(
      !is_interface_storage_rebindable_v<std::unique_ptr<std::shared_ptr<A>>,
                                         I>);

  static_assert(
      std::is_same_v<nested_stored_type, std::shared_ptr<std::unique_ptr<A>>>);

  static_assert(std::is_empty_v<conversion_cache<type_list<>>>);
  static_assert(
      std::is_trivially_destructible_v<conversion_cache<type_list<>>>);
  static_assert(sizeof(shared_binding) <= sizeof(shared_ptr_binding));
  static_assert(sizeof(external_binding) <= sizeof(external_shared_binding));
}

TEST(type_registration_test, runtime_multi_interface_binding_shares_storage) {
  dingo::container<> container;
  container.register_type<scope<shared>,
                          storage<runtime_binding_shared_implementation>,
                          interfaces<runtime_binding_first_interface,
                                     runtime_binding_second_interface>>();

  auto &first = container.resolve<runtime_binding_first_interface &>();
  auto &second = container.resolve<runtime_binding_second_interface &>();
  auto *first_impl =
      dynamic_cast<runtime_binding_shared_implementation *>(&first);
  auto *second_impl =
      dynamic_cast<runtime_binding_shared_implementation *>(&second);

  ASSERT_NE(first_impl, nullptr);
  EXPECT_EQ(first_impl, second_impl);
}

TEST(type_registration_test,
     runtime_shared_interface_conversion_returns_cached_handle) {
  dingo::container<> container;
  container.register_type<
      scope<shared>,
      storage<std::shared_ptr<runtime_binding_shared_implementation>>,
      interfaces<runtime_binding_first_interface,
                 runtime_binding_second_interface>>();

  auto &first =
      container.resolve<std::shared_ptr<runtime_binding_first_interface> &>();
  auto &second =
      container.resolve<std::shared_ptr<runtime_binding_first_interface> &>();

  EXPECT_EQ(std::addressof(first), std::addressof(second));
  EXPECT_EQ(first.get(), second.get());
}

TEST(type_registration_test,
     runtime_scoped_source_allocation_is_destroyed_when_resolution_throws) {
  runtime_scoped_source_dependency::destructions = 0;

  dingo::container<> container;
  container.register_type<scope<shared>,
                          storage<runtime_scoped_source_throwing_service>>();
  container.register_type<scope<unique>,
                          storage<runtime_scoped_source_dependency>>();

  EXPECT_THROW((container.resolve<runtime_scoped_source_throwing_service &>()),
               std::runtime_error);
  EXPECT_GT(runtime_scoped_source_dependency::destructions, 0);
}

TEST(type_registration_test,
     runtime_shared_storage_retains_unique_dependency_source) {
  runtime_retained_source_unique_dependency::live_count = 0;
  runtime_retained_source_unique_dependency::destructions = 0;
  runtime_retained_source_shared_service::should_throw = false;
  runtime_retained_source_shared_service::dependency_was_live_at_destruction =
      false;

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_retained_source_shared_service>,
        dependencies<runtime_retained_source_unique_dependency &>>();
    container.register_type<
        scope<unique>, storage<runtime_retained_source_unique_dependency>>();

    auto &service =
        container.resolve<runtime_retained_source_shared_service &>();
    EXPECT_TRUE(service.dependency_is_live());
    EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 1);
  }

  EXPECT_TRUE(runtime_retained_source_shared_service::
                  dependency_was_live_at_destruction);
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_shared_storage_retains_unique_pointer_dependency_source) {
  runtime_retained_source_pointer_dependency::live_count = 0;
  runtime_retained_source_pointer_service::dependency_was_live_at_destruction =
      false;

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_retained_source_pointer_service>,
        dependencies<runtime_retained_source_pointer_dependency *>>();
    container.register_type<
        scope<unique>, storage<runtime_retained_source_pointer_dependency *>,
        factory<runtime_retained_source_pointer_factory>>();

    auto &service =
        container.resolve<runtime_retained_source_pointer_service &>();
    EXPECT_TRUE(service.dependency_is_live());
    EXPECT_EQ(runtime_retained_source_pointer_dependency::live_count, 1);
  }

  EXPECT_TRUE(runtime_retained_source_pointer_service::
                  dependency_was_live_at_destruction);
  EXPECT_EQ(runtime_retained_source_pointer_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_retained_source_dependency_rolls_back_after_failed_shared) {
  runtime_retained_source_unique_dependency::live_count = 0;
  runtime_retained_source_unique_dependency::destructions = 0;
  runtime_retained_source_shared_service::should_throw = true;
  runtime_retained_source_shared_service::dependency_was_live_at_destruction =
      false;

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_retained_source_shared_service>,
      dependencies<runtime_retained_source_unique_dependency &>>();
  container.register_type<scope<unique>,
                          storage<runtime_retained_source_unique_dependency>>();

  EXPECT_THROW((container.resolve<runtime_retained_source_shared_service &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 0);

  runtime_retained_source_shared_service::should_throw = false;
  auto &service = container.resolve<runtime_retained_source_shared_service &>();
  EXPECT_TRUE(service.dependency_is_live());
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 1);
}

TEST(type_registration_test,
     committed_runtime_retained_source_survives_later_failed_transaction) {
  runtime_retained_source_unique_dependency::live_count = 0;
  runtime_retained_source_unique_dependency::destructions = 0;
  runtime_retained_source_shared_service::should_throw = false;
  runtime_retained_source_shared_service::dependency_was_live_at_destruction =
      false;
  runtime_scoped_local_dependency::constructions = 0;
  runtime_scoped_local_dependency::destructions = 0;
  runtime_scoped_local_service::should_throw = true;

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_retained_source_shared_service>,
      dependencies<runtime_retained_source_unique_dependency &>>();
  container.register_type<scope<unique>,
                          storage<runtime_retained_source_unique_dependency>>();
  container.register_type<scope<shared>, storage<runtime_scoped_local_service>,
                          dependencies<runtime_scoped_local_dependency &>>();
  container
      .register_type<scope<unique>, storage<runtime_scoped_local_dependency>>();

  auto &service = container.resolve<runtime_retained_source_shared_service &>();
  ASSERT_TRUE(service.dependency_is_live());

  EXPECT_THROW((container.resolve<runtime_scoped_local_service &>()),
               std::runtime_error);

  EXPECT_TRUE(service.dependency_is_live());
  EXPECT_EQ(std::addressof(service),
            std::addressof(
                container.resolve<runtime_retained_source_shared_service &>()));
  EXPECT_EQ(runtime_retained_source_unique_dependency::live_count, 1);
}

TEST(type_registration_test,
     runtime_retained_source_storage_resets_before_retained_sources) {
  std::vector<int> events;
  runtime_retained_source_order_dependency::events = std::addressof(events);

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_retained_source_order_service>,
        dependencies<runtime_retained_source_order_dependency &>>();
    container.register_type<
        scope<unique>, storage<runtime_retained_source_order_dependency>>();

    auto &service =
        container.resolve<runtime_retained_source_order_service &>();
    ASSERT_TRUE(service.dependency_is_live());
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
  runtime_retained_source_order_dependency::events = nullptr;
}

TEST(type_registration_test,
     runtime_policy_unique_shared_unique_keeps_shared_committed) {
  runtime_policy_nested_unique_dependency::live_count = 0;

  {
    dingo::container<> container;
    container.register_type<
        scope<unique>, storage<runtime_policy_outer_unique_service>,
        dependencies<runtime_policy_nested_shared_service &>>();
    container.register_type<
        scope<shared>, storage<runtime_policy_nested_shared_service>,
        dependencies<runtime_policy_nested_unique_dependency &>>();
    container.register_type<scope<unique>,
                            storage<runtime_policy_nested_unique_dependency>>();

    auto outer = container.resolve<runtime_policy_outer_unique_service>();
    EXPECT_TRUE(outer.shared_dependency_is_live());
    EXPECT_EQ(runtime_policy_nested_unique_dependency::live_count, 1);

    auto &shared = container.resolve<runtime_policy_nested_shared_service &>();
    EXPECT_EQ(std::addressof(shared), outer.shared_);
    EXPECT_TRUE(shared.dependency_is_live());
    EXPECT_EQ(runtime_policy_nested_unique_dependency::live_count, 1);
  }

  EXPECT_EQ(runtime_policy_nested_unique_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_policy_shared_value_unique_dependency_is_ephemeral) {
  runtime_policy_value_unique_dependency::live_count = 0;
  runtime_policy_value_unique_dependency::destructions = 0;

  {
    dingo::container<> container;
    container.register_type<
        scope<shared>, storage<runtime_policy_value_shared_service>,
        dependencies<runtime_policy_value_unique_dependency>>();
    container.register_type<scope<unique>,
                            storage<runtime_policy_value_unique_dependency>>();

    auto &service = container.resolve<runtime_policy_value_shared_service &>();
    EXPECT_EQ(service.value, 29);
    EXPECT_EQ(runtime_policy_value_unique_dependency::live_count, 0);
    EXPECT_GT(runtime_policy_value_unique_dependency::destructions, 0);

    auto &again = container.resolve<runtime_policy_value_shared_service &>();
    EXPECT_EQ(std::addressof(again), std::addressof(service));
    EXPECT_EQ(runtime_policy_value_unique_dependency::live_count, 0);
  }

  EXPECT_EQ(runtime_policy_value_unique_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_local_resolution_container_rolls_back_after_failed_resolution) {
  runtime_scoped_local_dependency::constructions = 0;
  runtime_scoped_local_dependency::destructions = 0;
  runtime_scoped_local_service::should_throw = true;

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_scoped_local_service>,
      dependencies<runtime_scoped_local_dependency &>,
      dingo::bindings<dingo::bind<scope<shared>,
                                  storage<runtime_scoped_local_dependency>>>>();

  EXPECT_THROW((container.resolve<runtime_scoped_local_service &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_scoped_local_dependency::constructions, 1);
  EXPECT_EQ(runtime_scoped_local_dependency::destructions, 1);

  runtime_scoped_local_service::should_throw = false;
  (void)container.resolve<runtime_scoped_local_service &>();
  EXPECT_EQ(runtime_scoped_local_dependency::constructions, 2);
  EXPECT_EQ(runtime_scoped_local_dependency::destructions, 1);
}

TEST(type_registration_test,
     first_failed_shared_resolve_resets_partial_storage) {
  runtime_shared_retry_service::construction_attempts = 0;
  runtime_shared_retry_service::should_throw = true;

  dingo::container<> container;
  container
      .register_type<scope<shared>, storage<runtime_shared_retry_service>>();

  EXPECT_THROW((container.resolve<runtime_shared_retry_service &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_shared_retry_service::construction_attempts, 1);

  runtime_shared_retry_service::should_throw = false;
  (void)container.resolve<runtime_shared_retry_service &>();
  EXPECT_EQ(runtime_shared_retry_service::construction_attempts, 2);
}

TEST(type_registration_test,
     later_failed_dependent_resolve_preserves_committed_shared_storage) {
  runtime_shared_state_dependency::constructions = 0;
  runtime_shared_state_dependency::destructions = 0;

  dingo::container<> container;
  container
      .register_type<scope<shared>, storage<runtime_shared_state_dependency>>();

  auto &committed = container.resolve<runtime_shared_state_dependency &>();
  EXPECT_EQ(runtime_shared_state_dependency::constructions, 1);
  EXPECT_EQ(runtime_shared_state_dependency::destructions, 0);

  container.register_type<scope<shared>,
                          storage<runtime_shared_state_throwing_consumer>,
                          dependencies<runtime_shared_state_dependency &>>();

  EXPECT_THROW((container.resolve<runtime_shared_state_throwing_consumer &>()),
               std::runtime_error);
  EXPECT_EQ(runtime_shared_state_dependency::constructions, 1);
  EXPECT_EQ(runtime_shared_state_dependency::destructions, 0);
  EXPECT_EQ(
      std::addressof(container.resolve<runtime_shared_state_dependency &>()),
      std::addressof(committed));
}

TEST(type_registration_test,
     failed_first_shared_cyclical_resolve_rolls_back_publication) {
  runtime_cyclical_rollback_implementation::should_throw = true;

  dingo::container<> container;
  container.register_type<
      scope<shared_cyclical>,
      storage<std::shared_ptr<runtime_cyclical_rollback_implementation>>,
      interfaces<runtime_cyclical_rollback_interface,
                 runtime_cyclical_rollback_implementation>>();

  EXPECT_THROW(
      (container
           .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>()),
      std::runtime_error);

  runtime_cyclical_rollback_implementation::should_throw = false;
  auto &resolved =
      container
          .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>();
  EXPECT_NE(resolved.get(), nullptr);
}

TEST(type_registration_test,
     later_failed_resolve_preserves_committed_shared_cyclical_storage) {
  runtime_cyclical_rollback_implementation::should_throw = false;

  dingo::container<> container;
  container.register_type<
      scope<shared_cyclical>,
      storage<std::shared_ptr<runtime_cyclical_rollback_implementation>>,
      interfaces<runtime_cyclical_rollback_interface,
                 runtime_cyclical_rollback_implementation>>();

  auto &committed =
      container
          .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>();

  container.register_type<
      scope<shared>, storage<runtime_cyclical_rollback_consumer>,
      dependencies<std::shared_ptr<runtime_cyclical_rollback_interface> &>>();
  EXPECT_THROW((container.resolve<runtime_cyclical_rollback_consumer &>()),
               std::runtime_error);

  auto &after_failure =
      container
          .resolve<std::shared_ptr<runtime_cyclical_rollback_interface> &>();
  EXPECT_EQ(std::addressof(after_failure), std::addressof(committed));
  EXPECT_EQ(after_failure.get(), committed.get());
}

TEST(type_registration_test,
     runtime_context_construction_policy_stack_routes_nested_lifetimes) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  {
    {
      container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
      runtime_transaction transaction(runtime, scratch);
      {
        runtime_context context(scratch, transaction);
        (void)context.construct<runtime_arena_tracker>(&events, 1);
        {
          auto persistent = context.use_persistent_construction();
          (void)persistent;
          (void)context.construct<runtime_arena_tracker>(&events, 2);
          {
            auto ephemeral = context.use_ephemeral_construction();
            (void)ephemeral;
            (void)context.construct<runtime_arena_tracker>(&events, 3);
          }
        }
        transaction.commit();
      }

      EXPECT_EQ(events, (std::vector<int>{3, 1}));
    }
  }

  EXPECT_EQ(events, (std::vector<int>{3, 1, 2}));
}

TEST(type_registration_test,
     runtime_context_detection_construction_uses_active_policy) {
  std::vector<int> events;
  runtime_policy_detected_tracker::events = std::addressof(events);

  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime_transaction transaction(runtime, scratch);
    {
      dingo::container<> container;
      runtime_context context(scratch, transaction);

      runtime_policy_detected_tracker::next_value = 1;
      (void)context
          .construct<runtime_policy_detected_tracker &, detail::automatic>(
              container);
      {
        auto persistent = context.use_persistent_construction();
        (void)persistent;
        runtime_policy_detected_tracker::next_value = 2;
        (void)context
            .construct<runtime_policy_detected_tracker &, detail::automatic>(
                container);
      }
      transaction.commit();
    }

    EXPECT_EQ(events, (std::vector<int>{1}));
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
  runtime_policy_detected_tracker::events = nullptr;
}

TEST(type_registration_test,
     runtime_context_ephemeral_policy_uses_scratch_inside_persistent_scope) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime_transaction transaction(runtime, scratch);
    {
      runtime_context context(scratch, transaction);
      {
        auto persistent = context.use_persistent_construction();
        (void)persistent;
        (void)context.construct<runtime_arena_tracker>(&events, 2);
        {
          auto ephemeral = context.use_ephemeral_construction();
          (void)ephemeral;
          (void)context.construct<runtime_arena_tracker>(&events, 1);
        }
      }
      transaction.commit();
    }

    EXPECT_EQ(events, (std::vector<int>{1}));
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(type_registration_test,
     runtime_context_scratch_destructor_runs_once_on_commit) {
  runtime_scoped_source_dependency::destructions = 0;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction transaction(runtime, scratch);
  {
    runtime_context context(scratch, transaction);
    (void)context.construct<runtime_scoped_source_dependency>();
    transaction.commit();
  }

  EXPECT_EQ(runtime_scoped_source_dependency::destructions, 1);
}

TEST(type_registration_test,
     runtime_context_scratch_destructor_runs_once_on_rollback) {
  runtime_scoped_source_dependency::destructions = 0;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    {
      runtime_context context(scratch, transaction);
      (void)context.construct<runtime_scoped_source_dependency>();
    }
  }

  EXPECT_EQ(runtime_scoped_source_dependency::destructions, 1);
}

TEST(type_registration_test,
     runtime_context_ephemeral_store_preserves_pending_transaction_actions) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    {
      runtime_context context(scratch, transaction);
      (void)context.construct<runtime_arena_tracker>(&events, 1);
      context.on_rollback([&events]() noexcept { events.push_back(2); });
    }

    EXPECT_EQ(events, (std::vector<int>{1}));
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(type_registration_test,
     successful_runtime_registration_keeps_state_until_container_destruction) {
  int factory_destructions = 0;
  {
    dingo::container<> container;
    runtime_registration_lifetime_factory factory(&factory_destructions);
    container.register_type<scope<shared>,
                            storage<runtime_registration_lifetime_service>>(
        std::move(factory));
    (void)container.resolve<runtime_registration_lifetime_service &>();
    EXPECT_EQ(factory_destructions, 0);
  }

  EXPECT_EQ(factory_destructions, 1);
}

TEST(type_registration_test,
     nested_runtime_registration_rolls_back_with_throwing_root_operation) {
  dingo::container<> container;

  EXPECT_THROW((container.construct<nested_transaction_outer_service>(
                   callable([&container]() -> nested_transaction_outer_service {
                     container.register_type<
                         scope<shared>,
                         storage<nested_transaction_registered_service>>();
                     throw std::runtime_error("root construction failed");
                   }))),
               std::runtime_error);

  EXPECT_THROW((container.resolve<nested_transaction_registered_service &>()),
               type_not_found_exception);

  container.register_type<scope<shared>,
                          storage<nested_transaction_registered_service>>();
  EXPECT_NO_THROW(
      (container.resolve<nested_transaction_registered_service &>()));
}

TEST(type_registration_test,
     nested_runtime_resolution_commits_with_root_resolution) {
  nested_transaction_resolved_service::constructions = 0;
  nested_transaction_resolved_service::destructions = 0;

  {
    dingo::container<> container;
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolved_service>>();
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolving_service>>(
        callable([&container] {
          return nested_transaction_resolving_service{std::addressof(
              container.resolve<nested_transaction_resolved_service &>())};
        }));

    auto &outer = container.resolve<nested_transaction_resolving_service &>();
    auto &dependency =
        container.resolve<nested_transaction_resolved_service &>();

    EXPECT_EQ(outer.dependency, std::addressof(dependency));
    EXPECT_EQ(nested_transaction_resolved_service::constructions, 1);
    EXPECT_EQ(nested_transaction_resolved_service::destructions, 0);
  }

  EXPECT_EQ(nested_transaction_resolved_service::destructions, 1);
}

TEST(type_registration_test,
     nested_runtime_resolution_rolls_back_with_throwing_root_resolution) {
  nested_transaction_resolved_service::constructions = 0;
  nested_transaction_resolved_service::destructions = 0;

  {
    dingo::container<> container;
    bool should_throw = true;
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolved_service>>();
    container.register_type<scope<shared>,
                            storage<nested_transaction_resolving_service>>(
        callable([&container, &should_throw] {
          auto &dependency =
              container.resolve<nested_transaction_resolved_service &>();
          if (should_throw) {
            throw std::runtime_error("root resolution failed");
          }
          return nested_transaction_resolving_service{
              std::addressof(dependency)};
        }));

    EXPECT_THROW((container.resolve<nested_transaction_resolving_service &>()),
                 std::runtime_error);
    EXPECT_EQ(nested_transaction_resolved_service::constructions, 1);
    EXPECT_EQ(nested_transaction_resolved_service::destructions, 1);

    should_throw = false;
    auto &outer = container.resolve<nested_transaction_resolving_service &>();
    auto &dependency =
        container.resolve<nested_transaction_resolved_service &>();

    EXPECT_EQ(outer.dependency, std::addressof(dependency));
    EXPECT_EQ(nested_transaction_resolved_service::constructions, 2);
    EXPECT_EQ(nested_transaction_resolved_service::destructions, 1);
  }

  EXPECT_EQ(nested_transaction_resolved_service::destructions, 2);
}

TEST(type_registration_test,
     failed_duplicate_runtime_registration_rolls_back_constructed_state) {
  int first_factory_destructions = 0;
  int duplicate_factory_destructions = 0;
  {
    dingo::container<> container;
    runtime_registration_lifetime_factory first(&first_factory_destructions);
    runtime_registration_lifetime_factory duplicate(
        &duplicate_factory_destructions);

    container.register_type<scope<shared>,
                            storage<runtime_registration_lifetime_service>>(
        std::move(first));
    EXPECT_THROW(
        (container.register_type<
            scope<shared>, storage<runtime_registration_lifetime_service>>(
            std::move(duplicate))),
        lookup_already_registered_exception);

    EXPECT_EQ(first_factory_destructions, 0);
    EXPECT_EQ(duplicate_factory_destructions, 1);
  }

  EXPECT_EQ(first_factory_destructions, 1);
  EXPECT_EQ(duplicate_factory_destructions, 1);
}

TEST(type_registration_test,
     runtime_binding_state_constructs_child_container_on_demand) {
  counting_runtime_binding_container parent(nullptr);
  counting_runtime_binding_container::constructions = 0;

  struct owner {
    using allocator_type = counting_runtime_binding_container::allocator_type;
    using parent_container_type = counting_runtime_binding_container;

    explicit owner(parent_container_type *parent)
        : allocator(parent->get_allocator()),
          runtime_storage(parent->get_allocator()), parent_storage(parent) {}

    container_runtime<allocator_type> &runtime() { return runtime_storage; }
    parent_container_type *parent() { return parent_storage; }
    allocator_type &get_allocator() { return allocator; }

    allocator_type allocator;
    container_runtime<allocator_type> runtime_storage;
    parent_container_type *parent_storage;
  } state_owner(&parent);

  runtime_binding_state<owner, counting_runtime_binding_container,
                        counting_runtime_binding_storage>
      state(&state_owner);

  EXPECT_EQ(counting_runtime_binding_container::constructions, 0);
  (void)state.storage();
  EXPECT_EQ(counting_runtime_binding_container::constructions, 0);

  auto &container = state.container();
  EXPECT_EQ(counting_runtime_binding_container::constructions, 1);
  EXPECT_EQ(std::addressof(state.container()), std::addressof(container));
  EXPECT_EQ(counting_runtime_binding_container::constructions, 1);
}

TEST(type_registration_test,
     materialized_runtime_binding_container_resolves_local_overrides) {
  struct dependency {
    int value;
  };
  struct service {
    explicit service(dependency &value) : dependency_value(value.value) {}

    int dependency_value;
  };

  dependency parent_dependency{1};
  dependency local_dependency{2};
  dingo::container<> container;
  container.register_type<scope<external>, storage<dependency &>>(
      parent_dependency);
  auto local = container.register_type<scope<shared>, storage<service>,
                                       dependencies<dependency &>>();
  local->register_type<scope<external>, storage<dependency &>>(
      local_dependency);

  EXPECT_EQ(container.resolve<service &>().dependency_value, 2);
}

TEST(type_registration_test,
     runtime_unique_registration_constructs_nested_container_on_demand) {
  runtime_unique_local_dependency::live_count = 0;

  dingo::container<> container;
  container.register_type<
      scope<unique>, storage<runtime_unique_local_service>,
      dependencies<runtime_unique_local_dependency &>,
      dingo::bindings<dingo::bind<
          scope<unique>, storage<runtime_unique_local_dependency>,
          factory<function<make_runtime_unique_local_dependency>>>>>();

  auto service = container.resolve<runtime_unique_local_service>();
  EXPECT_EQ(service.value, 13);
  EXPECT_TRUE(service.dependency_was_live);
  EXPECT_EQ(runtime_unique_local_dependency::live_count, 0);
}

TEST(type_registration_test,
     runtime_local_bindings_use_separate_resolution_container) {
  using empty_local_registration =
      type_registration<scope<shared>, storage<runtime_binding_local_service>,
                        dingo::bindings<>>;
  using empty_local_model = detail::binding_model<empty_local_registration>;
  static_assert(std::is_void_v<typename empty_local_model::bindings_type>);
  using empty_resolution_container =
      detail::runtime_binding_resolution_container_t<
          dingo::container<>, dingo::container<>,
          typename empty_local_model::bindings_type>;
  using empty_runtime_state = detail::runtime_binding_state_t<
      typename dingo::container<>::registry_type, dingo::container<>,
      typename empty_local_model::storage_type,
      typename empty_local_model::bindings_type>;
  static_assert(std::is_same_v<empty_resolution_container, dingo::container<>>);
  static_assert(std::is_same_v<typename empty_runtime_state::container_type,
                               dingo::container<>>);
  static_assert(
      std::is_same_v<typename empty_runtime_state::resolution_container_type,
                     dingo::container<>>);

  using non_empty_local_registration = type_registration<
      scope<shared>, storage<runtime_binding_local_service>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_binding_local_dependency>,
          factory<function<make_runtime_binding_local_dependency>>>>>;
  using non_empty_local_model =
      detail::binding_model<non_empty_local_registration>;
  static_assert(!std::is_void_v<typename non_empty_local_model::bindings_type>);
  using non_empty_resolution_container =
      detail::runtime_binding_resolution_container_t<
          typename dingo::container<>::registry_type::parent_container_type,
          dingo::container<>, typename non_empty_local_model::bindings_type>;
  using non_empty_local_binding =
      dingo::bind<scope<shared>, storage<runtime_binding_local_dependency>,
                  factory<function<make_runtime_binding_local_dependency>>>;
  using non_empty_runtime_state = detail::runtime_binding_state_t<
      typename dingo::container<>::registry_type, dingo::container<>,
      typename non_empty_local_model::storage_type,
      typename non_empty_local_model::bindings_type>;
  static_assert(
      !std::is_same_v<non_empty_resolution_container, dingo::container<>>);
  static_assert(std::is_same_v<typename non_empty_runtime_state::container_type,
                               dingo::container<>>);
  static_assert(std::is_same_v<
                typename non_empty_runtime_state::resolution_container_type,
                non_empty_resolution_container>);
  static_assert(sizeof(non_empty_resolution_container) <
                sizeof(detail::binding_scope<non_empty_local_binding>));

  dingo::container<> container;
  container.register_type<
      scope<shared>, storage<runtime_binding_local_service>,
      dependencies<runtime_binding_local_dependency &>,
      dingo::bindings<dingo::bind<
          scope<shared>, storage<runtime_binding_local_dependency>,
          factory<function<make_runtime_binding_local_dependency>>>>>();

  EXPECT_EQ(container.resolve<runtime_binding_local_service &>().value, 7);
}

TEST(type_registration_test,
     binding_model_rewrites_single_interface_storage_leaf) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};

  using registration =
      type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                        interfaces<I>>;
  using model = detail::binding_model<registration>;

  static_assert(model::storage_tag_is_complete);
  static_assert(model::use_interface_as_stored_leaf);
  static_assert(std::is_same_v<typename model::stored_leaf_type, I>);
  static_assert(
      std::is_same_v<typename model::stored_type, std::shared_ptr<I>>);
  static_assert(std::is_same_v<typename model::storage_type::stored_type,
                               std::shared_ptr<I>>);
  static_assert(model::valid);
}

TEST(type_registration_test,
     binding_model_preserves_storage_for_multi_interface_registration) {
  struct I {
    virtual ~I() = default;
  };
  struct J {
    virtual ~J() = default;
  };
  struct A : I, J {};

  using registration =
      type_registration<scope<shared>, storage<std::shared_ptr<A>>,
                        interfaces<I, J>>;
  using model = detail::binding_model<registration>;
  using expansion = detail::binding_expansion<model>;

  static_assert(model::storage_tag_is_complete);
  static_assert(!model::use_interface_as_stored_leaf);
  static_assert(std::is_same_v<typename model::stored_leaf_type, A>);
  static_assert(
      std::is_same_v<typename model::stored_type, std::shared_ptr<A>>);
  static_assert(
      std::is_same_v<
          typename expansion::interface_bindings,
          type_list<detail::binding<I, model>, detail::binding<J, model>>>);
  static_assert(model::valid);
}

TEST(type_registration_test, recursive_leaf_and_rebind_traits) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};

  using nested_handle = const std::shared_ptr<std::unique_ptr<A>> &;
  using nested_list = type_list<nested_handle, std::unique_ptr<A *>>;

  static_assert(std::is_same_v<leaf_type_t<nested_handle>, A>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_handle, I>,
                               std::shared_ptr<std::unique_ptr<I>> &>);
  static_assert(
      std::is_same_v<rebind_type_t<nested_handle, I>, std::shared_ptr<I> &>);
  static_assert(std::is_same_v<leaf_type_t<nested_list>, type_list<A, A>>);
  static_assert(std::is_same_v<rebind_leaf_t<nested_list, I>,
                               type_list<std::shared_ptr<std::unique_ptr<I>> &,
                                         std::unique_ptr<I *>>>);
}

TEST(type_registration_test, normalized_type_trait) {
  struct I {
    virtual ~I() = default;
  };
  struct A : I {};
  struct annotation_tag {};
  using selected_key = detail::type_selector<annotation_tag>;

  static_assert(
      std::is_same_v<
          normalized_type_t<const std::shared_ptr<std::unique_ptr<A>> &>, A>);
  static_assert(
      std::is_same_v<normalized_type_t<annotated<I &, annotation_tag>>,
                     annotated<I, annotation_tag>>);
  static_assert(!request_may_escape_v<A>);
  static_assert(!request_may_escape_v<A &&>);
  static_assert(request_may_escape_v<A &>);
  static_assert(request_may_escape_v<const A &>);
  static_assert(request_may_escape_v<A *>);
  static_assert(request_may_escape_v<annotated<A &, annotation_tag>>);
  static_assert(request_may_escape_v<const A *>);
  static_assert(
      request_may_escape_v<dependency<A &, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<const A &, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<A *, key_type<annotation_tag>>>);
  static_assert(
      request_may_escape_v<dependency<const A *, key_type<annotation_tag>>>);
  static_assert(!request_may_escape_v<dependency<A, key_type<annotation_tag>>>);
  static_assert(
      !request_may_escape_v<dependency<A &&, key_type<annotation_tag>>>);
  static_assert(request_may_escape_v<detail::selected<A &, selected_key>>);
  static_assert(
      request_may_escape_v<detail::selected<const A &, selected_key>>);
  static_assert(request_may_escape_v<detail::selected<A *, selected_key>>);
  static_assert(
      request_may_escape_v<detail::selected<const A *, selected_key>>);
  static_assert(!request_may_escape_v<detail::selected<A, selected_key>>);
  static_assert(!request_may_escape_v<detail::selected<A &&, selected_key>>);
}
