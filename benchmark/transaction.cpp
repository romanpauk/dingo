//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The transaction of a runtime resolution apart from a container: one with
// no action, and one that commits or rolls back the actions deferred to it.
// Its memory is of an arena, as that of a container of the run suite is.
// The run suite of benchmark/nbench_conf.py measures these cases with the
// scenarios.

#include <dingo/memory/arena_allocator.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/memory.h>

#include <nbench/bench.hpp>

#include <cstdint>
#include <memory>

namespace {

using allocator_type = dingo::arena_allocator<char>;

// The memory of a transaction's runtime: an arena of its own.
struct memory_fixture {
  unsigned char buffer[4096];
  dingo::arena<> arena{buffer};
  allocator_type allocator{arena};
};

struct transaction_fixture : memory_fixture {
  dingo::inline_arena<512> scratch;
  dingo::runtime_memory<allocator_type> runtime{allocator};
  dingo::runtime_transaction<allocator_type> transaction{runtime, scratch};
};

// An action a transaction defers, which the compiler cannot remove.
void deferred() noexcept { asm volatile("" : : : "memory"); }

void empty_transaction(nbench::State &state) {
  memory_fixture memory;
  dingo::runtime_memory<allocator_type> runtime(memory.allocator);
  while (state.keep_running()) {
    dingo::execute_transaction(runtime,
                               [&](auto &context) { nbench::keep(context); });
  }
}

// Commits or rolls back a transaction of as many actions as the argument,
// of a batch of transactions made apart from what is measured.
template <bool Commit> void finish_transaction(nbench::State &state) {
  constexpr int batch = 64;
  const std::uint64_t actions = state.arg(0);
  std::unique_ptr<transaction_fixture> made[batch];
  while (state.keep_running()) {
    state.pause();
    for (auto &each : made) {
      each = std::make_unique<transaction_fixture>();
      for (std::uint64_t action = 0; action != actions; ++action) {
        each->transaction.defer_rollback(deferred);
      }
    }
    state.resume();
    for (auto &each : made) {
      if constexpr (Commit) {
        each->transaction.commit();
      } else {
        each->transaction.rollback();
      }
    }
    state.pause();
    for (auto &each : made) {
      each.reset();
    }
    state.resume();
  }
  state.operations(batch);
}

// The lifetime of a transaction: made, deferred as many actions as the
// argument, committed.
void commit_transaction(nbench::State &state) {
  const std::uint64_t actions = state.arg(0);
  memory_fixture memory;
  dingo::runtime_memory<allocator_type> runtime(memory.allocator);
  while (state.keep_running()) {
    dingo::inline_arena<512> scratch;
    dingo::runtime_transaction<allocator_type> transaction(runtime, scratch);
    for (std::uint64_t action = 0; action != actions; ++action) {
      transaction.defer_rollback(deferred);
    }
    transaction.commit();
  }
}

const nbench::args action_counts{{0}, {1}, {2}, {4}, {8}, {16}};

NBENCH(empty_transaction, "transaction/empty");
// A batch made apart from what is measured, a fixed number of times.
const nbench::iterations batches{64};

NBENCH(finish_transaction<true>, "transaction/commit", action_counts, batches);
NBENCH(finish_transaction<false>, "transaction/rollback", action_counts,
       batches);
NBENCH(commit_transaction, "transaction/lifetime", action_counts);

} // namespace
