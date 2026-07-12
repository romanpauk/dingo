//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/resolution/cache.h>
#include <dingo/resolution/conversion_cache.h>
#include <dingo/storage/shared.h>

#include <gtest/gtest.h>

#include <functional>
#include <memory>
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
  template <typename Fn> void on_rollback(Fn &&fn) {
    rollback_actions.emplace_back(std::forward<Fn>(fn));
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
  using detail::cache::state<Enabled>::get;
  using detail::cache::state<Enabled>::set;
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

void publish_address(void *state, void *address) {
  *static_cast<void **>(state) = address;
}

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

TEST(cache_test, sink_publishes_address_when_connected) {
  int value = 42;
  void *published = nullptr;
  detail::cache::sink sink{std::addressof(published), &publish_address};

  sink(std::addressof(value));

  EXPECT_EQ(published, std::addressof(value));
  EXPECT_NO_THROW(detail::cache::sink{}(std::addressof(value)));
}

TEST(cache_test, state_stores_entries_only_when_enabled) {
  detail::cache::entry entry{detail::cache::key<int *>(), nullptr};
  exposed_cache_state<true> enabled;
  exposed_cache_state<false> disabled;

  EXPECT_EQ(enabled.get(), nullptr);
  enabled.set(std::addressof(entry));
  ASSERT_NE(enabled.get(), nullptr);
  EXPECT_NE(enabled.get(), std::addressof(entry));
  EXPECT_EQ(enabled.get()->key, entry.key);
  EXPECT_EQ(enabled.get()->address, entry.address);
  enabled.set(nullptr);
  EXPECT_EQ(enabled.get(), nullptr);

  disabled.set(nullptr);
  EXPECT_EQ(disabled.get(), nullptr);

  static_assert(sizeof(exposed_cache_state<true>) == sizeof(entry));
  static_assert(std::is_empty_v<exposed_cache_state<false>>);
}

TEST(cache_test, stable_storage_trait_requires_explicit_stability) {
  static_assert(detail::cache::is_stable_storage_v<stable_storage>);
  static_assert(!detail::cache::is_stable_storage_v<unstable_storage>);
  static_assert(!detail::cache::is_stable_storage_v<unmarked_storage>);
}

TEST(cache_test, conversion_cache_reuses_value_until_reset) {
  conversion_cache<type_list<cache_value>> cache;
  cache_owner owner;

  auto &first = cache.construct<cache_value>(persistent_scope, owner, 1);
  auto &again = cache.construct<cache_value>(persistent_scope, owner, 2);

  EXPECT_EQ(std::addressof(again), std::addressof(first));
  EXPECT_EQ(again.value, 1);
  ASSERT_EQ(owner.values.size(), 1u);

  cache.reset();
  auto &after_reset = cache.construct<cache_value>(persistent_scope, owner, 3);

  EXPECT_NE(std::addressof(after_reset), std::addressof(first));
  EXPECT_EQ(after_reset.value, 3);
  EXPECT_EQ(owner.values.size(), 2u);
}

TEST(cache_test, tracked_conversion_clears_slot_on_rollback) {
  conversion_cache<cache_value> cache;
  tracked_cache_context context;

  auto &first = cache.construct_tracked<cache_value>(context, 1);
  auto &again = cache.construct_tracked<cache_value>(context, 2);
  EXPECT_EQ(std::addressof(again), std::addressof(first));
  ASSERT_EQ(context.values.size(), 1u);

  context.rollback();
  auto &after_rollback = cache.construct_tracked<cache_value>(context, 3);

  EXPECT_NE(std::addressof(after_rollback), std::addressof(first));
  EXPECT_EQ(after_rollback.value, 3);
  EXPECT_EQ(context.values.size(), 2u);
}

TEST(cache_test, empty_conversion_cache_has_no_state) {
  static_assert(std::is_empty_v<conversion_cache<type_list<>>>);
  static_assert(
      std::is_trivially_destructible_v<conversion_cache<type_list<>>>);

  conversion_cache<type_list<>> cache;
  cache.reset();
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
