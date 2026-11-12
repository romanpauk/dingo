//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/binding/cache.h>
#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/retention.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {
namespace {

struct runtime_conversion_rollback_interface {
  virtual ~runtime_conversion_rollback_interface() = default;
};

struct runtime_conversion_rollback_implementation
    : runtime_conversion_rollback_interface {};

struct runtime_request_cache_rollback_consumer {
  explicit runtime_request_cache_rollback_consumer(int *) {
    throw std::runtime_error("resolution failed");
  }
};

struct runtime_conversion_rollback_consumer {
  explicit runtime_conversion_rollback_consumer(
      std::shared_ptr<runtime_conversion_rollback_interface> &handle) {
    if (first_handle == nullptr) {
      first_handle = std::addressof(handle);
    } else {
      second_handle = std::addressof(handle);
    }
    if (should_throw) {
      throw std::runtime_error("resolution failed");
    }
  }

  static bool should_throw;
  static std::shared_ptr<runtime_conversion_rollback_interface> *first_handle;
  static std::shared_ptr<runtime_conversion_rollback_interface> *second_handle;
};

// Takes a wrapper a request composes of a shared handle, and fails while it
// should.
struct composed_rollback_consumer {
  explicit composed_rollback_consumer(
      std::optional<std::shared_ptr<runtime_conversion_rollback_interface>>
          &handle) {
    seen = std::addressof(handle);
    if (should_throw) {
      throw std::runtime_error("resolution failed");
    }
  }

  static bool should_throw;
  static std::optional<std::shared_ptr<runtime_conversion_rollback_interface>>
      *seen;
};

bool composed_rollback_consumer::should_throw = true;
std::optional<std::shared_ptr<runtime_conversion_rollback_interface>>
    *composed_rollback_consumer::seen = nullptr;

// A wrapper of a copy of the handle a shared scope keeps is kept with it: a
// reference refers to one object, for as long as the container lives, which
// shares the instance.
template <typename Container> void expect_kept_composition(Container &c) {
  using leaf_handle =
      std::shared_ptr<runtime_conversion_rollback_implementation>;
  using leaf_wrapper = std::optional<leaf_handle>;
  using interface_wrapper =
      std::optional<std::shared_ptr<runtime_conversion_rollback_interface>>;
  auto &first = c.template resolve<leaf_wrapper &>();
  auto &again = c.template resolve<const leaf_wrapper &>();
  auto *pointer = c.template resolve<leaf_wrapper *>();
  auto &converted = c.template resolve<interface_wrapper &>();
  auto &kept = c.template resolve<leaf_handle &>();
  EXPECT_EQ(std::addressof(again), std::addressof(first));
  EXPECT_EQ(pointer, std::addressof(first));
  EXPECT_EQ(first->get(), kept.get());
  EXPECT_EQ(std::addressof(c.template resolve<interface_wrapper &>()),
            std::addressof(converted));
  EXPECT_EQ(static_cast<runtime_conversion_rollback_implementation *>(
                converted->get()),
            kept.get());
  auto value = c.template resolve<leaf_wrapper>();
  EXPECT_EQ(value->get(), kept.get());
}

bool runtime_conversion_rollback_consumer::should_throw = true;
std::shared_ptr<runtime_conversion_rollback_interface>
    *runtime_conversion_rollback_consumer::first_handle = nullptr;
std::shared_ptr<runtime_conversion_rollback_interface>
    *runtime_conversion_rollback_consumer::second_handle = nullptr;

struct cache_value {
  explicit cache_value(int init_value) : value(init_value) {}
  int value;
};

struct cache_owner {
  template <typename T, typename... Args>
  T &construct(construction_scope, Args &&...args) {
    static_assert(std::is_same_v<T, cache_value>);
    values.push_back(std::make_unique<T>(std::forward<Args>(args)...));
    return *values.back();
  }

  std::vector<std::unique_ptr<cache_value>> values;
};

struct tracked_cache_context : cache_owner {
  void on_rollback(void (*fn)(void *, void *) noexcept, void *first,
                   void *second = nullptr) {
    rollback_actions.emplace_back([fn, first, second] { fn(first, second); });
  }

  void rollback() {
    for (auto it = rollback_actions.rbegin(); it != rollback_actions.rend();
         ++it) {
      (*it)();
    }
    rollback_actions.clear();
  }

  std::vector<std::function<void()>> rollback_actions;
};

template <bool Enabled>
struct exposed_cache_state : detail::cache::state<Enabled> {
  using detail::cache::state<Enabled>::slot;
};

struct stable_storage {
  struct conversions {
    static constexpr bool is_stable = true;
  };
};

struct unstable_storage {
  struct conversions {
    static constexpr bool is_stable = false;
  };
};

struct unmarked_storage {};

TEST(cache_test, request_traits_and_keys_distinguish_cached_types) {
  static_assert(detail::cache::supports_v<int &>);
  static_assert(detail::cache::supports_v<const int &>);
  static_assert(detail::cache::supports_v<int *>);
  static_assert(!detail::cache::supports_v<int>);
  static_assert(!detail::cache::supports_v<int &&>);

  EXPECT_EQ(detail::cache::key<int *>(), detail::cache::key<int *>());
  EXPECT_NE(detail::cache::key<int *>(), detail::cache::key<const int *>());
  EXPECT_NE(detail::cache::key<int &>(), detail::cache::key<const int &>());
}

TEST(cache_test, state_has_a_slot_only_when_enabled) {
  detail::cache::entry entry{detail::cache::key<int *>(), nullptr};
  exposed_cache_state<true> enabled;
  exposed_cache_state<false> disabled;

  ASSERT_NE(enabled.slot(), nullptr);
  EXPECT_EQ(enabled.slot()->key, nullptr);
  EXPECT_EQ(disabled.slot(), nullptr);

  static_assert(sizeof(exposed_cache_state<true>) == sizeof(entry));
  static_assert(std::is_empty_v<exposed_cache_state<false>>);
}

TEST(cache_test, stable_storage_trait_requires_explicit_stability) {
  static_assert(detail::cache::is_stable_storage_v<stable_storage>);
  static_assert(!detail::cache::is_stable_storage_v<unstable_storage>);
  static_assert(!detail::cache::is_stable_storage_v<unmarked_storage>);
}

// What a static container's binding keeps it owns: one object per key, kept
// until it is cleared, and destroyed then.
TEST(cache_test, retention_keeps_one_object_per_key_until_cleared) {
  detail::retention kept;
  tracked_cache_context context;
  int key_a = 0;
  int key_b = 0;

  auto &first =
      kept.keep<cache_value>(&key_a, context, [] { return cache_value(1); });
  auto &again =
      kept.keep<cache_value>(&key_a, context, [] { return cache_value(2); });
  auto &other =
      kept.keep<cache_value>(&key_b, context, [] { return cache_value(3); });

  EXPECT_EQ(std::addressof(again), std::addressof(first));
  EXPECT_EQ(again.value, 1);
  EXPECT_NE(std::addressof(other), std::addressof(first));
  EXPECT_EQ(other.value, 3);

  kept.clear();
  auto &after_clear =
      kept.keep<cache_value>(&key_a, context, [] { return cache_value(4); });
  EXPECT_EQ(after_clear.value, 4);
}

TEST(cache_test,
     runtime_conversion_cache_entry_rolls_back_after_failed_resolution) {
  runtime_conversion_rollback_consumer::should_throw = true;
  runtime_conversion_rollback_consumer::first_handle = nullptr;
  runtime_conversion_rollback_consumer::second_handle = nullptr;

  dingo::container<> container;
  container.register_type<
      scope<shared>,
      storage<std::shared_ptr<runtime_conversion_rollback_implementation>>,
      interfaces<runtime_conversion_rollback_interface,
                 runtime_conversion_rollback_implementation>>();
  container.register_type<
      scope<shared>, storage<runtime_conversion_rollback_consumer>,
      dependencies<std::shared_ptr<runtime_conversion_rollback_interface> &>>();

  EXPECT_THROW((container.resolve<runtime_conversion_rollback_consumer &>()),
               std::runtime_error);
  ASSERT_NE(runtime_conversion_rollback_consumer::first_handle, nullptr);
  auto &implementation = container.resolve<
      std::shared_ptr<runtime_conversion_rollback_implementation> &>();
  EXPECT_EQ(implementation.use_count(), 1);

  runtime_conversion_rollback_consumer::should_throw = false;
  (void)container.resolve<runtime_conversion_rollback_consumer &>();
  ASSERT_NE(runtime_conversion_rollback_consumer::second_handle, nullptr);
  EXPECT_EQ(implementation.use_count(), 2);

  auto &first =
      container
          .resolve<std::shared_ptr<runtime_conversion_rollback_interface> &>();
  auto &second =
      container
          .resolve<std::shared_ptr<runtime_conversion_rollback_interface> &>();
  EXPECT_EQ(std::addressof(first), std::addressof(second));
}

TEST(cache_test,
     committed_runtime_conversion_cache_survives_later_failed_resolution) {
  runtime_conversion_rollback_consumer::should_throw = true;
  runtime_conversion_rollback_consumer::first_handle = nullptr;
  runtime_conversion_rollback_consumer::second_handle = nullptr;

  dingo::container<> container;
  container.register_type<
      scope<shared>,
      storage<std::shared_ptr<runtime_conversion_rollback_implementation>>,
      interfaces<runtime_conversion_rollback_interface,
                 runtime_conversion_rollback_implementation>>();

  auto &committed =
      container
          .resolve<std::shared_ptr<runtime_conversion_rollback_interface> &>();

  container.register_type<
      scope<shared>, storage<runtime_conversion_rollback_consumer>,
      dependencies<std::shared_ptr<runtime_conversion_rollback_interface> &>>();

  EXPECT_THROW((container.resolve<runtime_conversion_rollback_consumer &>()),
               std::runtime_error);

  auto &after_failure =
      container
          .resolve<std::shared_ptr<runtime_conversion_rollback_interface> &>();
  EXPECT_EQ(std::addressof(after_failure), std::addressof(committed));
  EXPECT_EQ(after_failure.get(), committed.get());
}

TEST(cache_test, runtime_binding_keeps_composed_wrapper) {
  dingo::container<> container;
  container.register_type<
      scope<shared>,
      storage<std::shared_ptr<runtime_conversion_rollback_implementation>>,
      interfaces<runtime_conversion_rollback_interface,
                 runtime_conversion_rollback_implementation>>();
  expect_kept_composition(container);
}

TEST(cache_test, static_binding_keeps_composed_wrapper) {
  static_container<bindings<dingo::bind<
      scope<shared>,
      storage<std::shared_ptr<runtime_conversion_rollback_implementation>>,
      interfaces<runtime_conversion_rollback_interface,
                 runtime_conversion_rollback_implementation>>>>
      container;
  expect_kept_composition(container);
}

// A wrapper a unique scope makes is the request's: a reference to it is
// refused to the caller.
TEST(cache_test, unique_binding_keeps_no_composed_wrapper) {
  dingo::container<> container;
  container.register_type<
      scope<unique>,
      storage<std::shared_ptr<runtime_conversion_rollback_implementation>>>();
  EXPECT_THROW(
      (container.resolve<std::optional<
           std::shared_ptr<runtime_conversion_rollback_implementation>> &>()),
      type_not_convertible_exception);
}

TEST(cache_test, runtime_composed_wrapper_rolls_back_after_failed_resolution) {
  composed_rollback_consumer::should_throw = true;
  composed_rollback_consumer::seen = nullptr;

  dingo::container<> container;
  container.register_type<
      scope<shared>,
      storage<std::shared_ptr<runtime_conversion_rollback_implementation>>,
      interfaces<runtime_conversion_rollback_interface,
                 runtime_conversion_rollback_implementation>>();
  container.register_type<scope<shared>, storage<composed_rollback_consumer>>();

  EXPECT_THROW((container.resolve<composed_rollback_consumer &>()),
               std::runtime_error);
  ASSERT_NE(composed_rollback_consumer::seen, nullptr);
  auto &implementation = container.resolve<
      std::shared_ptr<runtime_conversion_rollback_implementation> &>();
  // What the failed resolution kept is gone, the handle it copied with it.
  EXPECT_EQ(implementation.use_count(), 1);

  composed_rollback_consumer::should_throw = false;
  (void)container.resolve<composed_rollback_consumer &>();
  auto &kept = container.resolve<std::optional<
      std::shared_ptr<runtime_conversion_rollback_interface>> &>();
  EXPECT_EQ(std::addressof(kept), composed_rollback_consumer::seen);
  // The storage and the kept wrapper share the instance.
  EXPECT_EQ(implementation.use_count(), 2);
}

TEST(cache_test, rollback_clears_cache_replaced_by_another_request_shape) {
  dingo::container<> container;
  container.register_type<scope<shared>, storage<int>>();
  container.register_type<scope<shared>,
                          storage<runtime_request_cache_rollback_consumer>,
                          dependencies<int *>>();

  auto &committed = container.resolve<int &>();
  EXPECT_THROW((container.resolve<runtime_request_cache_rollback_consumer &>()),
               std::runtime_error);

  EXPECT_EQ(std::addressof(container.resolve<int &>()),
            std::addressof(committed));
}

} // namespace
} // namespace dingo
