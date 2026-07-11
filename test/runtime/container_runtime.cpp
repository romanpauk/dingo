//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/runtime/container_runtime.h>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace dingo {
namespace {

struct runtime_arena_tracker {
  explicit runtime_arena_tracker(std::vector<int> *events, int value)
      : events_(events), value_(value) {}

  ~runtime_arena_tracker() { events_->push_back(value_); }

  std::vector<int> *events_;
  int value_;
};

struct object_store_destroy_observer {
  object_store_destroy_observer(std::vector<int> *events, int *observed)
      : events_(events), observed_(observed) {}

  ~object_store_destroy_observer() { events_->push_back(*observed_); }

  std::vector<int> *events_;
  int *observed_;
};

TEST(container_runtime_test,
     container_runtime_destroys_constructed_objects_in_reverse_order) {
  std::vector<int> events;
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime.construct<runtime_arena_tracker>(&events, 1);
    runtime.construct<runtime_arena_tracker>(&events, 2);
  }

  ASSERT_EQ(events, (std::vector<int>{2, 1}));
}

TEST(container_runtime_test,
     container_runtime_rollback_destroys_objects_after_checkpoint) {
  std::vector<int> events;
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime.construct<runtime_arena_tracker>(&events, 1);
    auto checkpoint = runtime.mark();
    runtime.construct<runtime_arena_tracker>(&events, 2);
    runtime.construct<runtime_arena_tracker>(&events, 3);

    runtime.rollback(checkpoint);
    ASSERT_EQ(events, (std::vector<int>{3, 2}));
  }

  ASSERT_EQ(events, (std::vector<int>{3, 2, 1}));
}

TEST(container_runtime_test,
     container_runtime_rollback_destroys_before_rewinding_arena) {
  std::vector<int> events;
  container_runtime<std::allocator<char>> runtime(std::allocator<char>{});

  auto checkpoint = runtime.mark();
  int *observed = std::addressof(runtime.construct<int>(42));
  runtime.construct<object_store_destroy_observer>(&events, observed);

  runtime.rollback(checkpoint);

  ASSERT_EQ(events, (std::vector<int>{42}));
}

TEST(container_runtime_test,
     container_runtime_rollback_preserves_reverse_order_across_linked_nodes) {
  std::vector<int> events;
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    runtime.construct<runtime_arena_tracker>(&events, 0);
    auto checkpoint = runtime.mark();
    for (int value = 1; value != 40; ++value) {
      runtime.construct<runtime_arena_tracker>(&events, value);
    }

    runtime.rollback(checkpoint);
    ASSERT_EQ(events.size(), 39u);
    for (int value = 39; value != 0; --value) {
      EXPECT_EQ(events[39 - value], value);
    }
  }

  ASSERT_EQ(events.back(), 0);
}

TEST(container_runtime_test,
     container_runtime_rollback_checkpoint_leaves_valid_linked_tail) {
  std::vector<int> events;
  {
    container_runtime<std::allocator<char>> runtime(std::allocator<char>{});
    for (int value = 1; value != 33; ++value) {
      runtime.construct<runtime_arena_tracker>(&events, value);
    }
    auto checkpoint = runtime.mark();
    runtime.construct<runtime_arena_tracker>(&events, 100);

    runtime.rollback(checkpoint);
    EXPECT_EQ(events, (std::vector<int>{100}));
    events.clear();

    runtime.construct<runtime_arena_tracker>(&events, 200);
  }

  ASSERT_EQ(events.size(), 33u);
  EXPECT_EQ(events.front(), 200);
  for (int value = 32; value != 0; --value) {
    EXPECT_EQ(events[33 - value], value);
  }
}

} // namespace
} // namespace dingo
