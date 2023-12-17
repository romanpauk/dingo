//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/arena_allocator.h>
#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include <benchmark/benchmark.h>

BENCHMARK_MAIN();

namespace {
int ClassCounter = 0;
template <size_t N> class Class {
  public:
    Class() { ClassCounter++; }
    int GetCounter() { return ClassCounter; }
};

static void resolve_baseline_unique(benchmark::State& state) {
    int counter = 0;
    for (auto _ : state) {
        Class<0> cls;
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_unique(benchmark::State& state) {
    using namespace dingo;
    using container_type = container<ContainerTraits>;
    container_type container;
    container.template register_type<scope<unique>, storage<Class<0>>>();

    int counter = 0;
    for (auto _ : state) {
        auto cls = container.template resolve<Class<0>>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

static void resolve_baseline_shared(benchmark::State& state) {
    Class<0> cls;
    int counter = 0;
    for (auto _ : state)
        counter += cls.GetCounter();
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_shared(benchmark::State& state) {
    using namespace dingo;
    using container_type = container<ContainerTraits>;
    container_type container;
    container.template register_type<scope<shared>, storage<Class<0>>>();

    int counter = 0;
    for (auto _ : state) {
        auto& cls = container.template resolve<Class<0>&>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_external(benchmark::State& state) {
    using namespace dingo;
    using container_type = dingo::container<ContainerTraits>;
    container_type container;
    Class<0> c;
    container.template register_type<scope<external>, storage<Class<0>>>(c);

    int counter = 0;
    for (auto _ : state) {
        auto& cls = container.template resolve<Class<0>&>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void construct_baseline(benchmark::State& state) {
    using container_type = dingo::container<ContainerTraits>;
    container_type container;

    int counter = 0;
    for (auto _ : state) {
        auto&& cls = container.template construct<Class<0>>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void register_type(benchmark::State& state) {
    using namespace dingo;
    for (auto _ : state) {
        Container container;
        container.template register_type<scope<unique>, storage<Class<0>>>();
    }

    state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void register_type_10(benchmark::State& state) {
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
static void register_type_arena(benchmark::State& state) {
    using namespace dingo;
    arena<1024> arena;
    for (auto _ : state) {
        arena_allocator<char> allocator(arena);
        Container container(allocator);
        container.template register_type<scope<unique>, storage<Class<0>>>();
    }

    state.SetBytesProcessed(state.iterations());
}

template <typename Container>
static void register_type_arena_10(benchmark::State& state) {
    using namespace dingo;
    for (auto _ : state) {
        arena<8192> arena;
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

BENCHMARK(resolve_baseline_unique);
BENCHMARK_TEMPLATE(resolve_container_unique, dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_unique, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK(resolve_baseline_shared);
BENCHMARK_TEMPLATE(resolve_container_shared, dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_shared, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_external, dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_external, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(construct_baseline, dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(construct_baseline, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type,
                   dingo::container<dingo::static_container_traits<>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(register_type,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(register_type_arena,
                   dingo::container<dingo::dynamic_container_traits,
                                    dingo::arena_allocator<char>>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_10,
                   dingo::container<dingo::static_container_traits<>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(register_type_10,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_arena_10,
                   dingo::container<dingo::dynamic_container_traits,
                                    dingo::arena_allocator<char>>)
    ->UseRealTime();

} // namespace
