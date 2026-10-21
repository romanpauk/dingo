//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The transaction log of a container and the objects it keeps: a failed
// outermost request undoes the effects recorded during it, once each, in the
// reverse order they happened, and a request that returns keeps them; the
// kept objects live until their owner is destroyed, unless a failed
// transaction made them. spec: transaction_rules

#include <dingo/core/primitives/retained.h>
#include <dingo/core/primitives/slot.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace {

using dingo::core::retained_objects;
using dingo::core::transaction_log;

void record_event(void *events, void *value) noexcept {
  static_cast<std::vector<int> *>(events)->push_back(
      *static_cast<int *>(value));
}

struct failure {};

TEST(next_transactions_log,
     a_failed_request_undoes_its_effects_once_in_reverse_order) {
  std::vector<int> events;
  int first = 1;
  int second = 2;
  int third = 3;
  transaction_log log;
  EXPECT_THROW(log.run([&] {
    log.record(&events, &record_event, &first);
    // A nested request records into the transaction of the outermost one,
    // and its failure caught inside it undoes nothing yet.
    try {
      log.run([&]() -> int {
        log.record(&events, &record_event, &second);
        throw failure();
      });
    } catch (const failure &) {
    }
    EXPECT_TRUE(events.empty());
    log.record(&events, &record_event, &third);
    throw failure();
  }),
               failure);
  EXPECT_EQ(events, (std::vector<int>{3, 2, 1}));
  EXPECT_FALSE(log.active());
}

TEST(next_transactions_log, a_returning_request_keeps_its_effects) {
  std::vector<int> events;
  int value = 1;
  transaction_log log;
  EXPECT_EQ(log.run([&] {
    EXPECT_TRUE(log.active());
    log.record(&events, &record_event, &value);
    return 7;
  }),
            7);
  EXPECT_FALSE(log.active());
  // A later failure undoes only its own effects.
  EXPECT_THROW(log.run([&]() -> int { throw failure(); }), failure);
  EXPECT_TRUE(events.empty());
}

TEST(next_transactions_log, an_effect_outside_a_request_is_kept) {
  std::vector<int> events;
  int value = 1;
  transaction_log log;
  log.record(&events, &record_event, &value);
  EXPECT_THROW(log.run([&]() -> int { throw failure(); }), failure);
  EXPECT_TRUE(events.empty());
}

TEST(next_transactions_log, records_an_effect_once_when_asked_to) {
  std::vector<int> events;
  int value = 1;
  transaction_log log;
  EXPECT_THROW(log.run([&]() -> int {
    log.record_once(&events, &record_event, &value);
    log.record_once(&events, &record_event, &value);
    throw failure();
  }),
               failure);
  EXPECT_EQ(events, (std::vector<int>{1}));
}

// More effects than the log holds without allocating are undone too.
TEST(next_transactions_log, undoes_more_effects_than_it_holds_inline) {
  std::vector<int> events;
  int values[20];
  transaction_log log;
  EXPECT_THROW(log.run([&]() -> int {
    for (int index = 0; index < 20; ++index) {
      values[index] = index;
      log.record(&events, &record_event, &values[index]);
    }
    throw failure();
  }),
               failure);
  ASSERT_EQ(events.size(), 20u);
  EXPECT_EQ(events.front(), 19);
  EXPECT_EQ(events.back(), 0);
}

struct tracker {
  tracker(std::vector<int> *list, int id) : events(list), value(id) {}
  tracker(const tracker &) = delete;
  ~tracker() { events->push_back(value); }
  std::vector<int> *events;
  int value;
};

TEST(next_retained_objects,
     keeps_objects_until_destroyed_in_reverse_order_of_keeping) {
  std::vector<int> events;
  {
    retained_objects kept;
    kept.emplace<tracker>([&] { return tracker(&events, 1); });
    kept.emplace<tracker>([&] { return tracker(&events, 2); });
    EXPECT_TRUE(events.empty());
  }
  EXPECT_EQ(events, (std::vector<int>{2, 1}));
}

TEST(next_retained_objects,
     a_failed_transaction_destroys_once_the_objects_kept_during_it) {
  std::vector<int> events;
  {
    retained_objects kept;
    transaction_log log;
    kept.emplace<tracker>([&] { return tracker(&events, 1); });
    log.run([&] {
      kept.enter(log);
      kept.emplace<tracker>([&] { return tracker(&events, 2); });
    });
    EXPECT_TRUE(events.empty());
    EXPECT_THROW(log.run([&]() -> int {
      kept.enter(log);
      kept.emplace<tracker>([&] { return tracker(&events, 3); });
      kept.enter(log);
      kept.emplace<tracker>([&] { return tracker(&events, 4); });
      throw failure();
    }),
                 failure);
    EXPECT_EQ(events, (std::vector<int>{4, 3}));
  }
  EXPECT_EQ(events, (std::vector<int>{4, 3, 2, 1}));
}

} // namespace
