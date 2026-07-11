//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/transaction.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace dingo {
namespace {
using test_runtime_transaction = runtime_transaction<std::allocator<char>>;

struct runtime_arena_tracker {
  explicit runtime_arena_tracker(std::vector<int> *events, int value)
      : events_(events), value_(value) {}

  ~runtime_arena_tracker() { events_->push_back(value_); }

  std::vector<int> *events_;
  int value_;
};

template <typename Owner>
bool address_is_within(const Owner &owner, const void *ptr) {
  const auto address = reinterpret_cast<std::uintptr_t>(ptr);
  const auto begin = reinterpret_cast<std::uintptr_t>(std::addressof(owner));
  return begin <= address && address < begin + sizeof(owner);
}

struct runtime_ordered_persistent {
  runtime_ordered_persistent(std::vector<int> *events, int value, bool *alive)
      : events_(events), value_(value), alive_(alive) {
    *alive_ = true;
  }

  ~runtime_ordered_persistent() {
    *alive_ = false;
    events_->push_back(value_ * 10);
  }

  std::vector<int> *events_;
  int value_;
  bool *alive_;
};

struct runtime_scratch_observer {
  runtime_scratch_observer(std::vector<int> *events,
                           runtime_ordered_persistent *persistent,
                           bool *persistent_alive)
      : events_(events), persistent_(persistent),
        persistent_alive_(persistent_alive) {}

  ~runtime_scratch_observer() {
    events_->push_back(*persistent_alive_ ? persistent_->value_ : -1);
  }

  std::vector<int> *events_;
  runtime_ordered_persistent *persistent_;
  bool *persistent_alive_;
};

struct runtime_transaction_tracked_action {
  runtime_transaction_tracked_action(std::vector<int> *init_events,
                                     int init_action_value,
                                     int init_destroy_value)
      : events(init_events), action_value(init_action_value),
        destroy_value(init_destroy_value) {}

  runtime_transaction_tracked_action(
      runtime_transaction_tracked_action &&other) noexcept
      : events(other.events), action_value(other.action_value),
        destroy_value(other.destroy_value) {
    other.events = nullptr;
  }

  runtime_transaction_tracked_action(
      const runtime_transaction_tracked_action &) = delete;
  runtime_transaction_tracked_action &
  operator=(const runtime_transaction_tracked_action &) = delete;

  ~runtime_transaction_tracked_action() {
    if (events != nullptr) {
      events->push_back(destroy_value);
    }
  }

  void operator()() noexcept { events->push_back(action_value); }

  std::vector<int> *events;
  int action_value;
  int destroy_value;
};

struct runtime_transaction_reentrant_tracked_action {
  runtime_transaction_reentrant_tracked_action(
      test_runtime_transaction *init_transaction, std::vector<int> *init_events,
      int init_action_value, int init_destroy_value, bool init_install_action)
      : transaction(init_transaction), events(init_events),
        action_value(init_action_value), destroy_value(init_destroy_value),
        install_action(init_install_action) {}

  runtime_transaction_reentrant_tracked_action(
      runtime_transaction_reentrant_tracked_action &&other) noexcept
      : transaction(other.transaction), events(other.events),
        action_value(other.action_value), destroy_value(other.destroy_value),
        install_action(other.install_action) {
    other.events = nullptr;
  }

  runtime_transaction_reentrant_tracked_action(
      const runtime_transaction_reentrant_tracked_action &) = delete;
  runtime_transaction_reentrant_tracked_action &
  operator=(const runtime_transaction_reentrant_tracked_action &) = delete;

  ~runtime_transaction_reentrant_tracked_action() {
    if (events != nullptr) {
      events->push_back(destroy_value);
    }
  }

  void operator()() noexcept {
    events->push_back(action_value);
    if (install_action) {
      transaction->on_commit(runtime_transaction_reentrant_tracked_action(
          transaction, events, 3, 30, false));
    }
  }

  test_runtime_transaction *transaction;
  std::vector<int> *events;
  int action_value;
  int destroy_value;
  bool install_action;
};

TEST(runtime_transaction_test,
     runtime_transaction_rollback_allows_following_committed_allocation) {
  std::vector<int> events;
  {
    arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime.construct<runtime_arena_tracker>(&events, 0);
    events.clear();

    {
      runtime_transaction transaction(runtime, scratch);
      transaction.construct_persistent<runtime_arena_tracker>(&events, 1);
    }
    ASSERT_EQ(events, (std::vector<int>{1}));
    events.clear();

    runtime_transaction transaction(runtime, scratch);
    auto *committed =
        transaction.construct_persistent<runtime_arena_tracker>(&events, 2);
    transaction.commit();

    EXPECT_NE(committed, nullptr);
  }
  EXPECT_EQ(events, (std::vector<int>{2, 0}));
}

TEST(runtime_transaction_test,
     runtime_transaction_without_persistent_allocations_is_harmless) {
  std::vector<int> events;
  {
    arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime.construct<runtime_arena_tracker>(&events, 1);
    {
      runtime_transaction transaction(runtime, scratch);
      transaction.commit();
    }
    {
      runtime_transaction transaction(runtime, scratch);
    }
    EXPECT_TRUE(events.empty());
  }

  EXPECT_EQ(events, (std::vector<int>{1}));
}

TEST(runtime_transaction_test,
     runtime_transaction_destroys_action_states_after_commit_callbacks) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction transaction(runtime, scratch);

  transaction.on_commit(runtime_transaction_tracked_action(&events, 1, 10));
  transaction.on_commit(runtime_transaction_tracked_action(&events, 2, 20));
  transaction.commit();

  EXPECT_EQ(events, (std::vector<int>{2, 1, 20, 10}));
}

TEST(runtime_transaction_test,
     runtime_transaction_first_action_stays_in_inline_arena) {
  std::vector<int> events;
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_rollback([&events]() noexcept { events.push_back(1); });

    auto *remaining = scratch.allocate(208, alignof(void *));
    EXPECT_TRUE(address_is_within(scratch, remaining));
  }

  EXPECT_EQ(events, (std::vector<int>{1}));
}

TEST(runtime_transaction_test,
     runtime_transaction_nontrivial_action_stays_in_inline_arena) {
  std::vector<int> events;
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_rollback(runtime_transaction_tracked_action(&events, 1, 10));

    auto *remaining = scratch.allocate(48, alignof(std::max_align_t));
    EXPECT_TRUE(address_is_within(scratch, remaining));
  }

  EXPECT_EQ(events, (std::vector<int>{1, 10}));
}

TEST(runtime_transaction_test,
     runtime_transaction_processes_action_installed_by_completion) {
  std::vector<int> events;
  events.reserve(2);
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction transaction(runtime, scratch);

  transaction.on_commit([&transaction, &events]() noexcept {
    events.push_back(1);
    transaction.on_commit([&events]() noexcept { events.push_back(2); });
  });
  transaction.commit();

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(runtime_transaction_test,
     runtime_transaction_defers_reentrant_action_state_destruction) {
  std::vector<int> events;
  events.reserve(6);
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction transaction(runtime, scratch);

  transaction.on_commit(runtime_transaction_reentrant_tracked_action(
      &transaction, &events, 1, 10, false));
  transaction.on_commit(runtime_transaction_reentrant_tracked_action(
      &transaction, &events, 2, 20, true));
  transaction.commit();

  EXPECT_EQ(events, (std::vector<int>{2, 3, 1, 30, 20, 10}));
}

TEST(runtime_transaction_test,
     runtime_transaction_destroys_action_states_after_rollback_callbacks) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_rollback(runtime_transaction_tracked_action(&events, 1, 10));
    transaction.on_rollback(runtime_transaction_tracked_action(&events, 2, 20));
  }

  EXPECT_EQ(events, (std::vector<int>{2, 1, 20, 10}));
}

TEST(runtime_transaction_test, runtime_transaction_action_state_finishes_once) {
  std::vector<int> events;
  {
    arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    {
      runtime_transaction transaction(runtime, scratch);
      transaction.on_finish(runtime_transaction_tracked_action(&events, 4, 5));
    }
    {
      runtime_transaction transaction(runtime, scratch);
      transaction.on_finish(runtime_transaction_tracked_action(&events, 6, 7));
      transaction.commit();
    }
  }

  EXPECT_EQ(events, (std::vector<int>{4, 5, 6, 7}));
}

TEST(runtime_transaction_test,
     runtime_transaction_commit_preserves_reverse_action_order) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction transaction(runtime, scratch);

  for (int value = 1; value != 40; ++value) {
    transaction.on_commit(
        [&events, value]() noexcept { events.push_back(value); });
  }
  transaction.commit();

  ASSERT_EQ(events.size(), 39u);
  for (int value = 39; value != 0; --value) {
    EXPECT_EQ(events[39 - value], value);
  }
}

TEST(runtime_transaction_test,
     nested_runtime_transaction_commit_is_provisional_until_root_commit) {
  std::vector<int> events;
  arena<> outer_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  arena<> inner_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction outer(runtime, outer_scratch);
  outer.on_commit(runtime_transaction_tracked_action(&events, 1, 10));

  {
    runtime_transaction inner(runtime, inner_scratch);
    inner.on_commit(runtime_transaction_tracked_action(&events, 2, 20));
    inner.commit();
  }

  EXPECT_TRUE(events.empty());
  outer.commit();
  EXPECT_EQ(events, (std::vector<int>{2, 1, 20, 10}));
}

TEST(runtime_transaction_test,
     transactions_on_different_runtimes_finish_independently) {
  std::vector<int> events;
  arena<> outer_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  arena<> inner_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> outer_runtime(std::allocator<char>{});
  container_runtime<std::allocator<char>> inner_runtime(std::allocator<char>{});

  {
    runtime_transaction outer(outer_runtime, outer_scratch);
    outer.on_rollback([&events]() noexcept { events.push_back(1); });

    runtime_transaction inner(inner_runtime, inner_scratch);
    inner.on_commit([&events]() noexcept { events.push_back(2); });
    inner.commit();

    EXPECT_EQ(events, (std::vector<int>{2}));
  }

  EXPECT_EQ(events, (std::vector<int>{2, 1}));
}

TEST(runtime_transaction_test,
     nested_runtime_transaction_commit_rolls_back_with_root) {
  std::vector<int> events;
  arena<> outer_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  arena<> inner_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});

  {
    runtime_transaction outer(runtime, outer_scratch);
    outer.construct_persistent<runtime_arena_tracker>(&events, 1);
    outer.on_rollback([&events]() noexcept { events.push_back(10); });
    {
      runtime_transaction inner(runtime, inner_scratch);
      inner.construct_persistent<runtime_arena_tracker>(&events, 2);
      inner.on_rollback([&events]() noexcept { events.push_back(20); });
      inner.commit();
    }
  }

  EXPECT_EQ(events, (std::vector<int>{20, 10, 2, 1}));
}

TEST(runtime_transaction_test,
     nested_runtime_transaction_rollback_preserves_root_state) {
  std::vector<int> events;
  arena<> outer_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  arena<> inner_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction outer(runtime, outer_scratch);
  outer.construct_persistent<runtime_arena_tracker>(&events, 1);

  {
    runtime_transaction inner(runtime, inner_scratch);
    inner.construct_persistent<runtime_arena_tracker>(&events, 2);
  }

  EXPECT_EQ(events, (std::vector<int>{2}));
  outer.commit();
  EXPECT_EQ(events, (std::vector<int>{2}));
}

TEST(runtime_transaction_test,
     runtime_transaction_rollback_preserves_reverse_action_order) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  {
    runtime_transaction transaction(runtime, scratch);
    for (int value = 1; value != 40; ++value) {
      transaction.on_rollback(
          [&events, value]() noexcept { events.push_back(value); });
    }
  }

  ASSERT_EQ(events.size(), 39u);
  for (int value = 39; value != 0; --value) {
    EXPECT_EQ(events[39 - value], value);
  }
}

TEST(runtime_transaction_test,
     runtime_transaction_accepts_move_only_capturing_lambda) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
  runtime_transaction transaction(runtime, scratch);

  auto value = std::make_unique<int>(42);
  transaction.on_commit([&events, value = std::move(value)]() noexcept {
    events.push_back(*value);
  });
  transaction.commit();

  EXPECT_EQ(events, (std::vector<int>{42}));
}

TEST(runtime_transaction_test,
     runtime_transaction_rolls_back_without_runtime_context) {
  std::vector<int> events;
  runtime_arena_tracker *slot = nullptr;
  {
    arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    {
      runtime_transaction transaction(runtime, scratch);
      auto *created =
          transaction.construct_persistent<runtime_arena_tracker>(&events, 2);
      ASSERT_EQ(slot, nullptr);
      transaction.on_rollback([&slot]() noexcept { slot = nullptr; });
      slot = created;
      transaction.on_finish([&events]() noexcept { events.push_back(3); });
    }

    EXPECT_EQ(slot, nullptr);
    EXPECT_EQ(events, (std::vector<int>{3, 2}));
  }

  EXPECT_EQ(events, (std::vector<int>{3, 2}));
}

TEST(runtime_transaction_test,
     runtime_transaction_explicit_reset_action_covers_null_slot_install) {
  std::vector<int> events;
  runtime_ordered_persistent *slot = nullptr;
  {
    arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    {
      runtime_transaction transaction(runtime, scratch);
      bool installed_alive = false;
      auto *installed =
          transaction.construct_persistent<runtime_ordered_persistent>(
              &events, 2, &installed_alive);
      ASSERT_EQ(slot, nullptr);
      transaction.on_rollback([&slot]() noexcept { slot = nullptr; });
      slot = installed;
    }

    EXPECT_EQ(slot, nullptr);
    EXPECT_EQ(events, (std::vector<int>{20}));
  }
  EXPECT_EQ(events, (std::vector<int>{20}));
}

TEST(runtime_transaction_test,
     runtime_transaction_commit_keeps_persistent_state) {
  std::vector<int> events;
  runtime_arena_tracker *slot = nullptr;
  {
    arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    {
      runtime_transaction transaction(runtime, scratch);
      auto *created =
          transaction.construct_persistent<runtime_arena_tracker>(&events, 2);
      ASSERT_EQ(slot, nullptr);
      transaction.on_rollback([&slot]() noexcept { slot = nullptr; });
      slot = created;
      transaction.on_finish([&events]() noexcept { events.push_back(3); });
      transaction.commit();
    }

    ASSERT_NE(slot, nullptr);
    EXPECT_EQ(events, (std::vector<int>{3}));
  }

  EXPECT_EQ(events, (std::vector<int>{3, 2}));
}

TEST(runtime_transaction_test,
     runtime_transaction_finishes_scratch_before_persistent_rollback) {
  std::vector<int> events;
  {
    arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    {
      runtime_transaction transaction(runtime, scratch);
      bool persistent_alive = false;
      auto *persistent =
          transaction.construct_persistent<runtime_ordered_persistent>(
              &events, 7, &persistent_alive);
      auto *observer = new (scratch.allocate(sizeof(runtime_scratch_observer),
                                             alignof(runtime_scratch_observer)))
          runtime_scratch_observer(&events, persistent, &persistent_alive);
      auto destroy_observer = [observer]() noexcept {
        observer->~runtime_scratch_observer();
      };
      transaction.on_finish(destroy_observer);
    }

    EXPECT_EQ(events, (std::vector<int>{7, 70}));
  }

  EXPECT_EQ(events, (std::vector<int>{7, 70}));
}

TEST(runtime_transaction_test, actions_run_only_for_the_matching_outcome) {
  std::vector<int> commit_events;
  std::vector<int> rollback_events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});

  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_commit(
        [&commit_events]() noexcept { commit_events.push_back(1); });
    transaction.on_rollback(
        [&commit_events]() noexcept { commit_events.push_back(2); });
    transaction.on_finish(
        [&commit_events]() noexcept { commit_events.push_back(3); });
    transaction.commit();
  }
  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_commit(
        [&rollback_events]() noexcept { rollback_events.push_back(1); });
    transaction.on_rollback(
        [&rollback_events]() noexcept { rollback_events.push_back(2); });
    transaction.on_finish(
        [&rollback_events]() noexcept { rollback_events.push_back(3); });
  }

  EXPECT_EQ(commit_events, (std::vector<int>{3, 1}));
  EXPECT_EQ(rollback_events, (std::vector<int>{3, 2}));
}

TEST(runtime_transaction_test,
     rollback_processes_action_installed_by_rollback_callback) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});

  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_rollback([&transaction, &events]() noexcept {
      events.push_back(1);
      transaction.on_rollback([&events]() noexcept { events.push_back(2); });
    });
  }

  EXPECT_EQ(events, (std::vector<int>{1, 2}));
}

TEST(runtime_transaction_test,
     inner_rollback_discards_inner_actions_and_preserves_outer_actions) {
  std::vector<int> events;
  arena<> outer_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  arena<> inner_scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});

  runtime_transaction outer(runtime, outer_scratch);
  outer.on_commit([&events]() noexcept { events.push_back(1); });
  {
    runtime_transaction inner(runtime, inner_scratch);
    inner.on_commit([&events]() noexcept { events.push_back(2); });
    inner.on_rollback([&events]() noexcept { events.push_back(3); });
  }

  EXPECT_EQ(events, (std::vector<int>{3}));
  outer.commit();
  EXPECT_EQ(events, (std::vector<int>{3, 1}));
}

TEST(runtime_transaction_test, repeated_finish_calls_are_harmless) {
  int commit_finishes = 0;
  int rollback_finishes = 0;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});

  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_finish([&commit_finishes]() noexcept { ++commit_finishes; });
    transaction.commit();
    transaction.commit();
    transaction.rollback();
  }
  {
    runtime_transaction transaction(runtime, scratch);
    transaction.on_finish(
        [&rollback_finishes]() noexcept { ++rollback_finishes; });
    transaction.rollback();
    transaction.rollback();
    transaction.commit();
  }

  EXPECT_EQ(commit_finishes, 1);
  EXPECT_EQ(rollback_finishes, 1);
}

TEST(runtime_transaction_test,
     construct_persistent_with_registers_destructor_for_rollback) {
  std::vector<int> events;
  arena<> scratch(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});

  {
    runtime_transaction transaction(runtime, scratch);
    auto *value = transaction.construct_persistent_with<runtime_arena_tracker>(
        [&events](void *storage) {
          new (storage) runtime_arena_tracker(&events, 9);
        });
    EXPECT_NE(value, nullptr);
  }

  EXPECT_EQ(events, (std::vector<int>{9}));
}

} // namespace
} // namespace dingo
