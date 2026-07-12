//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/core/config.h>
#include <dingo/memory/object_store.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>
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

struct object_store_trivial_value {
  explicit object_store_trivial_value(int init_value) : value(init_value) {}

  int value;
};

static_assert(std::is_trivially_destructible_v<object_store_trivial_value>);

template <typename Owner>
bool address_is_within(const Owner &owner, const void *ptr) {
  const auto address = reinterpret_cast<std::uintptr_t>(ptr);
  const auto begin = reinterpret_cast<std::uintptr_t>(std::addressof(owner));
  return begin <= address && address < begin + sizeof(owner);
}

TEST(object_store_test,
     object_store_destroys_constructed_objects_in_reverse_order) {
  std::vector<int> events;
  arena<> backing(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  {
    detail::object_store<arena<>> store;
    store.construct<runtime_arena_tracker>(backing, &events, 1);
    store.construct<runtime_arena_tracker>(backing, &events, 2);
    store.destroy(backing);
  }

  ASSERT_EQ(events, (std::vector<int>{2, 1}));
}

TEST(object_store_test,
     object_store_first_destructor_record_stays_in_inline_arena) {
  std::vector<int> events;
  inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
  {
    detail::object_store<arena<>> store;
    store.construct<runtime_arena_tracker>(scratch, &events, 1);

    auto *remaining = scratch.allocate(48, alignof(std::max_align_t));
    EXPECT_TRUE(address_is_within(scratch, remaining));
    store.destroy(scratch);
  }

  EXPECT_EQ(events, (std::vector<int>{1}));
}

TEST(object_store_test, object_store_destroy_checkpoint_does_not_rewind_arena) {
  std::vector<int> events;
  arena<> backing(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  detail::object_store<arena<>> store;

  auto checkpoint = store.mark();
  int *observed = std::addressof(store.construct<int>(backing, 42));
  store.construct<object_store_destroy_observer>(backing, &events, observed);

  store.destroy(backing, checkpoint);

  ASSERT_EQ(events, (std::vector<int>{42}));
  EXPECT_NE(std::addressof(store.construct<int>(backing, 7)), observed);
}

TEST(object_store_test,
     object_store_destroy_preserves_reverse_order_across_linked_nodes) {
  std::vector<int> events;
  arena<> backing(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  detail::object_store<arena<>> store;

  auto checkpoint = store.mark();
  for (int value = 1; value != 40; ++value) {
    store.construct<runtime_arena_tracker>(backing, &events, value);
  }

  store.destroy(backing, checkpoint);

  ASSERT_EQ(events.size(), 39u);
  for (int value = 39; value != 0; --value) {
    EXPECT_EQ(events[39 - value], value);
  }
}

TEST(object_store_test,
     object_store_trivial_construction_does_not_register_destructor_records) {
  arena<> backing(DINGO_CONTEXT_ARENA_BUFFER_SIZE);
  detail::object_store<arena<>> store;

  auto checkpoint = store.mark();
  auto &first = store.construct<object_store_trivial_value>(backing, 42);

  store.destroy(backing, checkpoint);

  EXPECT_EQ(first.value, 42);
}

} // namespace
} // namespace dingo
