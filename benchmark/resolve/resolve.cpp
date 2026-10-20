//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Resolution benchmarks over the public API alone, so the same source builds
// against the current and the next implementation: dingo_resolve_benchmark
// and dingo_next_resolve_benchmark. A runtime container holds the requested
// binding among `N` others, so the cost of finding it shows.

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace {

struct service {
  service() {}
  int value = 1;
};

template <std::size_t> struct filler {};

struct iface {
  virtual ~iface() = default;
  virtual int id() const = 0;
};

struct impl : iface {
  int id() const override { return 1; }
};

struct consumer {
  explicit consumer(service &dependency) : value(dependency.value) {}
  int value;
};

// Registers the shared bindings filler<0> to filler<N - 1>.
template <typename Container, std::size_t... Indices>
void register_fillers(Container &container,
                      std::index_sequence<Indices...> /*indices*/) {
  (container.template register_type<dingo::scope<dingo::shared>,
                                    dingo::storage<filler<Indices>>>(),
   ...);
}

template <std::size_t N>
void runtime_shared_reference(benchmark::State &state) {
  dingo::container<> container;
  register_fillers(container, std::make_index_sequence<N>());
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
  for (auto _ : state) {
    benchmark::DoNotOptimize(&container.resolve<service &>());
  }
}

template <std::size_t N> void runtime_unique_value(benchmark::State &state) {
  dingo::container<> container;
  register_fillers(container, std::make_index_sequence<N>());
  container
      .register_type<dingo::scope<dingo::unique>, dingo::storage<service>>();
  for (auto _ : state) {
    benchmark::DoNotOptimize(container.resolve<service>().value);
  }
}

template <std::size_t N>
void runtime_construct_dependency(benchmark::State &state) {
  dingo::container<> container;
  register_fillers(container, std::make_index_sequence<N>());
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
  for (auto _ : state) {
    benchmark::DoNotOptimize(container.construct<consumer>().value);
  }
}

// The traits of a keyed domain indexed by `Backend`.
template <typename Backend>
struct keyed_traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, iface, dingo::one, Backend>>;
};

template <std::size_t N, typename Backend = dingo::ordered>
void runtime_keyed(benchmark::State &state) {
  dingo::container<keyed_traits<Backend>> container;
  for (std::size_t index = 0; index < N; ++index) {
    container
        .template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<impl>, dingo::interfaces<iface>>(
            dingo::key_value{static_cast<int>(index)});
  }
  const int key = static_cast<int>(N / 2);
  for (auto _ : state) {
    benchmark::DoNotOptimize(&container.template resolve<iface &>(key));
  }
}

// As runtime_keyed, with every key value in turn, so the search is not
// predicted from the one before.
template <std::size_t N, typename Backend = dingo::ordered>
void runtime_keyed_cycling(benchmark::State &state) {
  dingo::container<keyed_traits<Backend>> container;
  for (std::size_t index = 0; index < N; ++index) {
    container
        .template register_type<dingo::scope<dingo::shared>,
                                dingo::storage<impl>, dingo::interfaces<iface>>(
            dingo::key_value{static_cast<int>(index)});
  }
  std::size_t next = 0;
  for (auto _ : state) {
    const int key = static_cast<int>((next = next * 37 + 11) % N);
    benchmark::DoNotOptimize(&container.template resolve<iface &>(key));
  }
}

struct collection_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::collection<iface>>;
};

template <std::size_t N> void runtime_collection(benchmark::State &state) {
  dingo::container<collection_traits> container;
  for (std::size_t index = 0; index < N; ++index) {
    container.register_type<dingo::scope<dingo::shared>, dingo::storage<impl>,
                            dingo::interfaces<iface>>();
  }
  for (auto _ : state) {
    auto values = container.resolve<std::vector<iface *>>();
    benchmark::DoNotOptimize(values.data());
  }
}

void static_shared_reference(benchmark::State &state) {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<filler<0>>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<filler<1>>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>>;
  dingo::static_container<bindings> container;
  for (auto _ : state) {
    benchmark::DoNotOptimize(&container.resolve<service &>());
  }
}

} // namespace

BENCHMARK_TEMPLATE(runtime_shared_reference, 0);
BENCHMARK_TEMPLATE(runtime_shared_reference, 32);
BENCHMARK_TEMPLATE(runtime_unique_value, 0);
BENCHMARK_TEMPLATE(runtime_unique_value, 32);
BENCHMARK_TEMPLATE(runtime_construct_dependency, 32);
BENCHMARK_TEMPLATE(runtime_keyed, 4);
BENCHMARK_TEMPLATE(runtime_keyed, 64);
BENCHMARK_TEMPLATE(runtime_keyed, 64, dingo::unordered);
BENCHMARK_TEMPLATE(runtime_keyed, 64, dingo::array<64>);
BENCHMARK_TEMPLATE(runtime_keyed_cycling, 64);
BENCHMARK_TEMPLATE(runtime_keyed_cycling, 64, dingo::unordered);
BENCHMARK_TEMPLATE(runtime_keyed_cycling, 64, dingo::array<64>);
BENCHMARK_TEMPLATE(runtime_collection, 8);
BENCHMARK(static_shared_reference);

BENCHMARK_MAIN();
