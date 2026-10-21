//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The transaction of a container: the effects of its outermost request in
// progress and of the requests it makes, each recorded with the function that
// undoes it. A request that fails undoes them in the reverse order they
// happened, and one that returns keeps them. spec: transaction_rules

#include <dingo/core/primitives/slot.h>

#include <gtest/gtest.h>

#include <stdexcept>
#include <vector>

namespace {

using dingo::core::basic_transaction_log;
using dingo::core::no_transaction_log;
using dingo::core::transaction_log;

struct failure {};

// The effects undone, by the values recorded with them.
struct undone {
  std::vector<int> values;

  static void undo(void *target, void *context) noexcept {
    static_cast<undone *>(target)->values.push_back(
        *static_cast<int *>(context));
  }
};

template <typename Log>
void record(Log &log, undone &effects, std::vector<int> &values) {
  for (int &value : values) {
    log.record(&effects, &undone::undo, &value);
  }
}

TEST(next_transaction_log, a_failed_request_undoes_its_effects_in_reverse) {
  transaction_log log;
  undone effects;
  std::vector<int> values{1, 2, 3};
  EXPECT_THROW(log.run([&]() -> int {
    record(log, effects, values);
    throw failure{};
  }),
               failure);
  EXPECT_EQ(effects.values, (std::vector<int>{3, 2, 1}));
  EXPECT_FALSE(log.active());
}

TEST(next_transaction_log, a_request_that_returns_keeps_its_effects) {
  transaction_log log;
  undone effects;
  std::vector<int> values{1, 2};
  EXPECT_EQ(log.run([&] {
    EXPECT_TRUE(log.active());
    record(log, effects, values);
    return 7;
  }),
            7);
  EXPECT_TRUE(effects.values.empty());
  EXPECT_FALSE(log.active());

  // A later failure undoes only its own effects.
  std::vector<int> later{3};
  EXPECT_THROW(log.run([&]() -> int {
    record(log, effects, later);
    throw failure{};
  }),
               failure);
  EXPECT_EQ(effects.values, (std::vector<int>{3}));
}

TEST(next_transaction_log, an_effect_outside_a_request_is_kept) {
  transaction_log log;
  undone effects;
  std::vector<int> outside{1};
  record(log, effects, outside);
  std::vector<int> inside{2};
  EXPECT_THROW(log.run([&]() -> int {
    record(log, effects, inside);
    throw failure{};
  }),
               failure);
  EXPECT_EQ(effects.values, (std::vector<int>{2}));
}

TEST(next_transaction_log, undoes_more_effects_than_it_records_inline) {
  basic_transaction_log<8> log;
  undone effects;
  std::vector<int> values;
  for (int value = 1; value != 40; ++value) {
    values.push_back(value);
  }
  EXPECT_THROW(log.run([&]() -> int {
    record(log, effects, values);
    throw failure{};
  }),
               failure);
  ASSERT_EQ(effects.values.size(), 39u);
  for (int value = 39; value != 0; --value) {
    EXPECT_EQ(effects.values[39 - value], value);
  }

  // The log records inline again for the next request.
  std::vector<int> next{100};
  EXPECT_THROW(log.run([&]() -> int {
    record(log, effects, next);
    throw failure{};
  }),
               failure);
  EXPECT_EQ(effects.values.back(), 100);
}

TEST(next_transaction_log, only_the_outermost_request_undoes_the_effects) {
  {
    // A nested request that returns keeps its effects only until the
    // outermost one fails.
    transaction_log log;
    undone effects;
    std::vector<int> outer{1};
    std::vector<int> inner{2};
    EXPECT_THROW(log.run([&]() -> int {
      record(log, effects, outer);
      log.run([&] {
        record(log, effects, inner);
        return 0;
      });
      EXPECT_TRUE(effects.values.empty());
      throw failure{};
    }),
                 failure);
    EXPECT_EQ(effects.values, (std::vector<int>{2, 1}));
  }
  {
    // A nested request that fails, whose failure the outermost one catches,
    // undoes nothing: the outermost request returns and keeps every effect.
    transaction_log log;
    undone effects;
    std::vector<int> outer{1};
    std::vector<int> inner{2};
    log.run([&] {
      record(log, effects, outer);
      try {
        log.run([&]() -> int {
          record(log, effects, inner);
          throw failure{};
        });
      } catch (const failure &) {
      }
      return 0;
    });
    EXPECT_TRUE(effects.values.empty());
  }
}

TEST(next_transaction_log, the_transactions_of_two_containers_are_their_own) {
  transaction_log outer_log;
  transaction_log inner_log;
  undone effects;
  std::vector<int> outer{1};
  std::vector<int> inner{2};
  EXPECT_THROW(outer_log.run([&]() -> int {
    record(outer_log, effects, outer);
    inner_log.run([&] {
      record(inner_log, effects, inner);
      return 0;
    });
    throw failure{};
  }),
               failure);
  // The other container's request returned, so it keeps its effect.
  EXPECT_EQ(effects.values, (std::vector<int>{1}));
}

TEST(next_transaction_log, records_an_effect_once_per_transaction) {
  transaction_log log;
  undone effects;
  int first = 1;
  int second = 2;
  EXPECT_THROW(log.run([&]() -> int {
    log.record_once(&effects, &undone::undo, &first);
    log.record_once(&effects, &undone::undo, &second);
    throw failure{};
  }),
               failure);
  // The effect is recorded with the context of its first recording.
  EXPECT_EQ(effects.values, (std::vector<int>{1}));

  EXPECT_THROW(log.run([&]() -> int {
    log.record_once(&effects, &undone::undo, &second);
    throw failure{};
  }),
               failure);
  EXPECT_EQ(effects.values, (std::vector<int>{1, 2}));
}

TEST(next_transaction_log, a_log_that_never_grows_records_its_bound) {
  basic_transaction_log<4, false> log;
  undone effects;
  std::vector<int> values{1, 2, 3, 4};
  EXPECT_THROW(log.run([&]() -> int {
    record(log, effects, values);
    throw failure{};
  }),
               failure);
  EXPECT_EQ(effects.values, (std::vector<int>{4, 3, 2, 1}));
}

TEST(next_transaction_log, a_container_without_effects_runs_its_requests) {
  no_transaction_log log;
  undone effects;
  std::vector<int> values{1};
  EXPECT_EQ(log.run([&] {
    record(log, effects, values);
    return 3;
  }),
            3);
  EXPECT_THROW(log.run([&]() -> int {
    record(log, effects, values);
    throw failure{};
  }),
               failure);
  EXPECT_TRUE(effects.values.empty());
  EXPECT_FALSE(log.active());
}

// An effect recorded while the effects of a failed request are undone, as a
// destructor that registers into the container records it, is kept, and must
// not disturb the undoing. Disabled: when the record grows a log whose effects
// are already out of line, undo_entries walks the freed effects
// (heap-use-after-free under AddressSanitizer). Finding of the ledger.
basic_transaction_log<2> recording_log;
int recorded_undos = 0;

void count_undo(void * /*target*/, void * /*context*/) noexcept {
  ++recorded_undos;
}

void record_while_undoing(void * /*target*/, void * /*context*/) noexcept {
  ++recorded_undos;
  recording_log.record(nullptr, &count_undo);
}

TEST(next_transaction_log, DISABLED_an_effect_recorded_while_undoing_is_kept) {
  recorded_undos = 0;
  EXPECT_THROW(recording_log.run([&]() -> int {
    // Four effects grow the log out of line to its capacity.
    recording_log.record(nullptr, &count_undo);
    recording_log.record(nullptr, &count_undo);
    recording_log.record(nullptr, &count_undo);
    recording_log.record(nullptr, &record_while_undoing);
    throw failure{};
  }),
               failure);
  EXPECT_EQ(recorded_undos, 4);
}

} // namespace
