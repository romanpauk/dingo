//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <benchmark/benchmark.h>

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
BENCHMARK_TEMPLATE(register_type,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_10,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();

} // namespace
