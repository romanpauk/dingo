//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/context.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <benchmark/benchmark.h>

#include <array>
#include <chrono>
#include <memory>
#include <vector>

BENCHMARK_MAIN();

namespace {

struct IClass {
  virtual ~IClass() {}
};

template <size_t = 0> struct Class : IClass {};

struct cold_leaf {};

struct cold_service {
  explicit cold_service(cold_leaf &) {}
};

struct transaction_fixture {
  using allocator_type = std::allocator<char>;

  allocator_type allocator;
  dingo::inline_arena<512> scratch;
  dingo::container_runtime<allocator_type> runtime{allocator};
  dingo::runtime_transaction<allocator_type> transaction{runtime, scratch};
};

bool is_empty(const std::string &val) { return val.empty(); }

bool is_empty(const int &val) { return val == 0; }

template <typename T> bool is_empty(const std::shared_ptr<T> &val) {
  return val.get() == 0;
}

template <typename ContainerTraits>
static void resolve_container_unique_int(benchmark::State &state) {
  using namespace dingo;
  using container_type = container<ContainerTraits>;
  container_type container;
  container.template register_type<scope<unique>, storage<int>>();

  size_t count = 0;
  for (auto _ : state) {
    count += is_empty(container.template resolve<int>());
  }
  benchmark::DoNotOptimize(count);
  state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_unique_string(benchmark::State &state) {
  using namespace dingo;
  using container_type = container<ContainerTraits>;
  container_type container;
  container.template register_type<scope<unique>, storage<std::string>,
                                   factory<constructor<std::string()>>>();
  size_t count = 0;
  for (auto _ : state) {
    count += is_empty(container.template resolve<std::string>());
  }
  benchmark::DoNotOptimize(count);
  state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_shared(benchmark::State &state) {
  using namespace dingo;
  using container_type = container<ContainerTraits>;
  container_type container;
  container.template register_type<scope<shared>, storage<int>>();

  size_t count = 0;
  for (auto _ : state) {
    count += is_empty(container.template resolve<int &>());
  }
  benchmark::DoNotOptimize(count);
  state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_shared_ptr(benchmark::State &state) {
  using namespace dingo;
  using container_type = container<ContainerTraits>;
  container_type container;
  container
      .template register_type<scope<shared>, storage<std::shared_ptr<int>>>();

  size_t count = 0;
  for (auto _ : state) {
    count += is_empty(container.template resolve<std::shared_ptr<int> &>());
  }
  benchmark::DoNotOptimize(count);
  state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void
resolve_container_shared_ptr_conversion_storage(benchmark::State &state) {
  using namespace dingo;
  using container_type = container<ContainerTraits>;
  container_type container;
  container.template register_type<
      scope<shared>, storage<std::shared_ptr<Class<>>>, interfaces<IClass>>();

  size_t count = 0;
  for (auto _ : state) {
    count += is_empty(container.template resolve<std::shared_ptr<IClass> &>());
  }
  benchmark::DoNotOptimize(count);
  state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void
resolve_container_shared_ptr_conversion_resolver(benchmark::State &state) {
  using namespace dingo;
  using container_type = container<ContainerTraits>;
  container_type container;
  container
      .template register_type<scope<shared>, storage<std::shared_ptr<Class<>>>,
                              interfaces<Class<>, IClass>>();
  size_t count = 0;
  for (auto _ : state) {
    count += is_empty(container.template resolve<std::shared_ptr<IClass> &>());
  }
  benchmark::DoNotOptimize(count);
  state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_external(benchmark::State &state) {
  using namespace dingo;
  using container_type = dingo::container<ContainerTraits>;
  container_type container;
  int c = 1;
  container.template register_type<scope<external>, storage<int>>(c);

  size_t count = 0;
  for (auto _ : state) {
    count += is_empty(container.template resolve<int &>());
  }
  benchmark::DoNotOptimize(count);
  state.SetBytesProcessed(state.iterations());
}

template <typename Setup, typename Resolve>
static void resolve_container_cold(benchmark::State &state, Setup setup,
                                   Resolve resolve) {
  using container_type = dingo::container<dingo::dynamic_container_traits>;
  constexpr size_t batch_size = 64;
  size_t count = 0;
  for (auto _ : state) {
    std::vector<std::unique_ptr<container_type>> containers;
    containers.reserve(batch_size);
    for (size_t index = 0; index != batch_size; ++index) {
      auto container = std::make_unique<container_type>();
      setup(*container);
      containers.push_back(std::move(container));
    }
    const auto start = std::chrono::steady_clock::now();
    for (auto &container : containers) {
      count += resolve(*container);
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    state.SetIterationTime(std::chrono::duration<double>(elapsed).count());
  }
  benchmark::DoNotOptimize(count);
  state.SetItemsProcessed(state.iterations() * batch_size);
}

static void resolve_container_cold_external(benchmark::State &state) {
  using namespace dingo;
  int value = 0;
  resolve_container_cold(
      state,
      [&](auto &container) {
        container.template register_type<scope<external>, storage<int>>(value);
      },
      [](auto &container) {
        return is_empty(container.template resolve<int &>());
      });
}

static void resolve_container_cold_external_value(benchmark::State &state) {
  using namespace dingo;
  int value = 0;
  resolve_container_cold(
      state,
      [&](auto &container) {
        container.template register_type<scope<external>, storage<int>>(value);
      },
      [](auto &container) {
        return is_empty(container.template resolve<int>());
      });
}

static void empty_runtime_transaction(benchmark::State &state) {
  std::allocator<char> allocator;
  dingo::container_runtime<std::allocator<char>> runtime(allocator);
  for (auto _ : state) {
    dingo::execute_transaction(runtime, [&](auto &context) {
      benchmark::DoNotOptimize(runtime);
      benchmark::DoNotOptimize(context);
      benchmark::ClobberMemory();
    });
  }
}

static void runtime_transaction_barrier(benchmark::State &state) {
  std::allocator<char> allocator;
  dingo::container_runtime<std::allocator<char>> runtime(allocator);
  for (auto _ : state) {
    benchmark::DoNotOptimize(runtime);
    benchmark::ClobberMemory();
  }
}

template <bool Commit, bool RollbackActions>
static void finish_runtime_transaction(benchmark::State &state) {
  constexpr size_t batch_size = 64;
  for (auto _ : state) {
    std::array<transaction_fixture, batch_size> transactions;
    for (auto &fixture : transactions) {
      for (int64_t action = 0; action != state.range(0); ++action) {
        if constexpr (RollbackActions) {
          fixture.transaction.on_rollback(
              []() noexcept { benchmark::ClobberMemory(); });
        } else {
          fixture.transaction.on_commit(
              []() noexcept { benchmark::ClobberMemory(); });
        }
      }
    }

    const auto start = std::chrono::steady_clock::now();
    for (auto &fixture : transactions) {
      if constexpr (Commit) {
        fixture.transaction.commit();
      } else {
        fixture.transaction.rollback();
      }
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    state.SetIterationTime(std::chrono::duration<double>(elapsed).count());
  }
  state.SetItemsProcessed(state.iterations() * batch_size);
}

static void transaction_action_counts(benchmark::internal::Benchmark *value) {
  for (int64_t count : {0, 1, 2, 4, 8, 16}) {
    value->Arg(count);
  }
}

static void commit_runtime_transaction(benchmark::State &state) {
  using allocator_type = std::allocator<char>;
  allocator_type allocator;
  dingo::container_runtime<allocator_type> runtime(allocator);
  for (auto _ : state) {
    dingo::inline_arena<512> scratch;
    dingo::runtime_transaction transaction(runtime, scratch);
    for (int64_t action = 0; action != state.range(0); ++action) {
      transaction.on_rollback([]() noexcept { benchmark::ClobberMemory(); });
    }
    transaction.commit();
  }
}

static void resolve_container_cold_shared(benchmark::State &state) {
  using namespace dingo;
  resolve_container_cold(
      state,
      [](auto &container) {
        container.template register_type<scope<shared>, storage<int>>();
      },
      [](auto &container) {
        return is_empty(container.template resolve<int &>());
      });
}

static void resolve_container_cold_dependency(benchmark::State &state) {
  using namespace dingo;
  resolve_container_cold(
      state,
      [](auto &container) {
        container.template register_type<scope<shared>, storage<cold_leaf>>();
        container
            .template register_type<scope<shared>, storage<cold_service>>();
      },
      [](auto &container) {
        return &container.template resolve<cold_service &>() == nullptr;
      });
}

template <typename Container>
static void register_type(benchmark::State &state) {
  using namespace dingo;
  for (auto _ : state) {
    Container container;
    container.template register_type<scope<unique>, storage<Class<0>>>();
  }

  state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void register_type_10(benchmark::State &state) {
  using namespace dingo;
  for (auto _ : state) {
    Container container;
    container.template register_type<scope<unique>, storage<Class<0>>>();
    container.template register_type<scope<unique>, storage<Class<1>>>();
    container.template register_type<scope<unique>, storage<Class<2>>>();
    container.template register_type<scope<unique>, storage<Class<3>>>();
    container.template register_type<scope<unique>, storage<Class<4>>>();
    container.template register_type<scope<unique>, storage<Class<5>>>();
    container.template register_type<scope<unique>, storage<Class<6>>>();
    container.template register_type<scope<unique>, storage<Class<7>>>();
    container.template register_type<scope<unique>, storage<Class<8>>>();
    container.template register_type<scope<unique>, storage<Class<9>>>();
  }

  state.SetBytesProcessed(state.iterations() * 10);
}

template <typename Container>
static void register_type_arena(benchmark::State &state) {
  using namespace dingo;
  uint8_t buffer[1024];
  for (auto _ : state) {
    arena<> arena(buffer);
    arena_allocator<char> allocator(arena);
    Container container(allocator);
    container.template register_type<scope<unique>, storage<Class<0>>>();
  }

  state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void register_type_arena_10(benchmark::State &state) {
  using namespace dingo;
  uint8_t buffer[8192];
  for (auto _ : state) {
    arena<> arena(buffer);
    arena_allocator<char> allocator(arena);
    Container container(allocator);
    container.template register_type<scope<unique>, storage<Class<0>>>();
    container.template register_type<scope<unique>, storage<Class<1>>>();
    container.template register_type<scope<unique>, storage<Class<2>>>();
    container.template register_type<scope<unique>, storage<Class<3>>>();
    container.template register_type<scope<unique>, storage<Class<4>>>();
    container.template register_type<scope<unique>, storage<Class<5>>>();
    container.template register_type<scope<unique>, storage<Class<6>>>();
    container.template register_type<scope<unique>, storage<Class<7>>>();
    container.template register_type<scope<unique>, storage<Class<8>>>();
    container.template register_type<scope<unique>, storage<Class<9>>>();
  }

  state.SetBytesProcessed(state.iterations() * 10);
}

BENCHMARK_TEMPLATE(resolve_container_unique_int,
                   dingo::dynamic_container_traits)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_unique_string,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared_ptr,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared_ptr_conversion_storage,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared_ptr_conversion_resolver,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_external, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK(resolve_container_cold_external)->Iterations(100)->UseManualTime();
BENCHMARK(resolve_container_cold_external_value)
    ->Iterations(100)
    ->UseManualTime();
BENCHMARK(resolve_container_cold_shared)->Iterations(100)->UseManualTime();
BENCHMARK(resolve_container_cold_dependency)->Iterations(100)->UseManualTime();
BENCHMARK(empty_runtime_transaction);
BENCHMARK(runtime_transaction_barrier);
BENCHMARK_TEMPLATE(finish_runtime_transaction, true, true)
    ->Apply(transaction_action_counts)
    ->Iterations(500)
    ->UseManualTime();
BENCHMARK_TEMPLATE(finish_runtime_transaction, true, false)
    ->Apply(transaction_action_counts)
    ->Iterations(500)
    ->UseManualTime();
BENCHMARK_TEMPLATE(finish_runtime_transaction, false, true)
    ->Apply(transaction_action_counts)
    ->Iterations(500)
    ->UseManualTime();
BENCHMARK(commit_runtime_transaction)->Apply(transaction_action_counts);

BENCHMARK_TEMPLATE(register_type,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_arena,
                   dingo::container<dingo::dynamic_container_traits,
                                    dingo::arena_allocator<char>>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_10,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_arena_10,
                   dingo::container<dingo::dynamic_container_traits,
                                    dingo::arena_allocator<char>>)
    ->UseRealTime();
} // namespace
