//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

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
[[maybe_unused]] int build_from_double_and_cstr(double, const char *) {
  return 0;
}

struct runtime_binding_local_dependency {
  explicit runtime_binding_local_dependency(int init_value)
      : value(init_value) {}

  int value;
};

[[maybe_unused]] runtime_binding_local_dependency
make_runtime_binding_local_dependency() {
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
  Type construct(construction_scope, Context &, Container &) const {
    return Type{};
  }

  template <typename Type, typename Context, typename Container>
  void construct(void *ptr, construction_scope, Context &, Container &) const {
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

[[maybe_unused]] runtime_unique_local_dependency
make_runtime_unique_local_dependency() {
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
  Type construct(construction_scope scope, Context &context,
                 Container &) const {
    static_assert(std::is_pointer_v<Type>);
    using object_type = std::remove_pointer_t<Type>;
    return std::addressof(context.template construct<object_type>(scope));
  }

  template <typename Type, typename Context, typename Container>
  void construct(void *ptr, construction_scope scope, Context &context,
                 Container &container) const {
    new (ptr) Type(construct<Type>(scope, context, container));
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
