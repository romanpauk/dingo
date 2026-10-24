//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The objects a container keeps until it is destroyed, and those a failed
// transaction of the container destroys: the objects kept after the mark the
// transaction recorded, in the reverse order they were kept.
// spec: transaction_rules

#include <dingo/memory/lifetime_slot.h>
#include <dingo/memory/retained.h>

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace {

using dingo::core::kept_objects;
using dingo::core::retained_objects;
using dingo::core::transaction_log;

struct failure {};

struct tracker {
  tracker(std::vector<int> *init_events, int init_value)
      : events(init_events), value(init_value) {}
  tracker(const tracker &) = delete;
  tracker &operator=(const tracker &) = delete;
  ~tracker() { events->push_back(value); }

  std::vector<int> *events;
  int value;
};

// Reads an object kept before it when it is destroyed.
struct observer {
  observer(std::vector<int> *init_events, int *init_observed)
      : events(init_events), observed(init_observed) {}
  observer(const observer &) = delete;
  observer &operator=(const observer &) = delete;
  ~observer() { events->push_back(*observed); }

  std::vector<int> *events;
  int *observed;
};

template <typename Type, typename... Args>
Type &keep(retained_objects &objects, Args... args) {
  return objects.emplace<Type>([&] { return Type(args...); });
}

void keep_tracker(retained_objects &objects, std::vector<int> &events,
                  int value) {
  objects.emplace<tracker>([&] { return tracker(&events, value); });
}

TEST(next_retained, destroys_its_objects_in_the_reverse_order_they_were_kept) {
  std::vector<int> events;
  {
    retained_objects objects;
    keep_tracker(objects, events, 1);
    keep_tracker(objects, events, 2);
    keep_tracker(objects, events, 3);
  }
  EXPECT_EQ(events, (std::vector<int>{3, 2, 1}));

  events.clear();
  retained_objects objects;
  keep_tracker(objects, events, 1);
  keep_tracker(objects, events, 2);
  objects.clear();
  EXPECT_EQ(events, (std::vector<int>{2, 1}));
  keep_tracker(objects, events, 3);
  objects.clear();
  EXPECT_EQ(events, (std::vector<int>{2, 1, 3}));
}

TEST(next_retained, a_failed_transaction_destroys_the_objects_kept_during_it) {
  std::vector<int> events;
  {
    retained_objects objects;
    transaction_log log;
    keep_tracker(objects, events, 0);
    EXPECT_THROW(log.run([&]() -> int {
      objects.enter(log);
      for (int value = 1; value != 40; ++value) {
        keep_tracker(objects, events, value);
      }
      throw failure{};
    }),
                 failure);
    ASSERT_EQ(events.size(), 39u);
    for (int value = 39; value != 0; --value) {
      EXPECT_EQ(events[39 - value], value);
    }
    events.clear();
  }
  // The object kept before the transaction is the container's.
  EXPECT_EQ(events, (std::vector<int>{0}));
}

TEST(next_retained, a_transaction_that_returns_keeps_its_objects) {
  std::vector<int> events;
  {
    retained_objects objects;
    transaction_log log;
    keep_tracker(objects, events, 1);
    log.run([&] {
      objects.enter(log);
      keep_tracker(objects, events, 2);
      return 0;
    });
    EXPECT_TRUE(events.empty());

    // A later failure destroys only what was kept during it.
    EXPECT_THROW(log.run([&]() -> int {
      objects.enter(log);
      keep_tracker(objects, events, 3);
      throw failure{};
    }),
                 failure);
    EXPECT_EQ(events, (std::vector<int>{3}));
    events.clear();
  }
  EXPECT_EQ(events, (std::vector<int>{2, 1}));
}

TEST(next_retained, a_transaction_after_a_failed_one_keeps_its_objects) {
  std::vector<int> events;
  {
    retained_objects objects;
    transaction_log log;
    keep_tracker(objects, events, 0);
    // A failed transaction that kept nothing destroys nothing.
    EXPECT_THROW(log.run([&]() -> int {
      objects.enter(log);
      throw failure{};
    }),
                 failure);
    EXPECT_TRUE(events.empty());
    EXPECT_THROW(log.run([&]() -> int {
      objects.enter(log);
      keep_tracker(objects, events, 1);
      throw failure{};
    }),
                 failure);
    EXPECT_EQ(events, (std::vector<int>{1}));
    events.clear();
    log.run([&] {
      objects.enter(log);
      keep_tracker(objects, events, 2);
      return 0;
    });
    EXPECT_TRUE(events.empty());
  }
  EXPECT_EQ(events, (std::vector<int>{2, 0}));
}

TEST(next_retained, objects_kept_after_an_undo_follow_the_remaining_ones) {
  std::vector<int> events;
  {
    retained_objects objects;
    transaction_log log;
    for (int value = 1; value != 33; ++value) {
      keep_tracker(objects, events, value);
    }
    EXPECT_THROW(log.run([&]() -> int {
      objects.enter(log);
      keep_tracker(objects, events, 100);
      throw failure{};
    }),
                 failure);
    EXPECT_EQ(events, (std::vector<int>{100}));
    events.clear();
    keep_tracker(objects, events, 200);
  }
  ASSERT_EQ(events.size(), 33u);
  EXPECT_EQ(events.front(), 200);
  for (int value = 32; value != 0; --value) {
    EXPECT_EQ(events[33 - value], value);
  }
}

TEST(next_retained, an_object_is_destroyed_before_the_objects_kept_before_it) {
  std::vector<int> events;
  retained_objects objects;
  transaction_log log;
  EXPECT_THROW(log.run([&]() -> int {
    objects.enter(log);
    int *observed = &keep<int>(objects, 42);
    objects.emplace<observer>([&] { return observer(&events, observed); });
    throw failure{};
  }),
               failure);
  EXPECT_EQ(events, (std::vector<int>{42}));
}

TEST(next_retained, enters_a_transaction_once_with_its_first_mark) {
  std::vector<int> events;
  retained_objects objects;
  transaction_log log;
  keep_tracker(objects, events, 1);
  EXPECT_THROW(log.run([&]() -> int {
    objects.enter(log);
    keep_tracker(objects, events, 2);
    // A nested request enters again; the mark stays the first one.
    log.run([&] {
      objects.enter(log);
      keep_tracker(objects, events, 3);
      return 0;
    });
    throw failure{};
  }),
               failure);
  EXPECT_EQ(events, (std::vector<int>{3, 2}));
}

TEST(next_retained, keeps_one_object_under_a_key) {
  kept_objects objects;
  int made = 0;
  const char first_key = 0;
  const char second_key = 0;
  auto make = [&made] {
    ++made;
    return made;
  };
  int &first = objects.get_or_emplace<int>(&first_key, make);
  EXPECT_EQ(&objects.get_or_emplace<int>(&first_key, make), &first);
  EXPECT_EQ(made, 1);
  int &second = objects.get_or_emplace<int>(&second_key, make);
  EXPECT_NE(&second, &first);
  EXPECT_EQ(made, 2);
  EXPECT_EQ(first, 1);
  EXPECT_EQ(second, 2);
}

TEST(next_retained, keeps_a_move_only_object) {
  retained_objects objects;
  auto &kept = objects.emplace<std::unique_ptr<int>>(
      [] { return std::make_unique<int>(5); });
  EXPECT_EQ(*kept, 5);
}

} // namespace
