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

struct IClass {
    virtual ~IClass() {}
};

template <size_t = 0> struct Class : IClass {};

bool is_empty(const std::string& val) { return val.empty(); }

bool is_empty(const int& val) { return val == 0; }

template <typename T> bool is_empty(const std::shared_ptr<T>& val) {
    return val.get() == 0;
}

template <typename ContainerTraits>
static void resolve_container_unique_int(benchmark::State& state) {
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
static void resolve_container_unique_string(benchmark::State& state) {
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
static void resolve_container_shared(benchmark::State& state) {
    using namespace dingo;
    using container_type = container<ContainerTraits>;
    container_type container;
    container.template register_type<scope<shared>, storage<int>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(container.template resolve<int&>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_shared_ptr(benchmark::State& state) {
    using namespace dingo;
    using container_type = container<ContainerTraits>;
    container_type container;
    container
        .template register_type<scope<shared>, storage<std::shared_ptr<int>>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(container.template resolve<std::shared_ptr<int>&>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void
resolve_container_shared_ptr_conversion_storage(benchmark::State& state) {
    using namespace dingo;
    using container_type = container<ContainerTraits>;
    container_type container;
    container.template register_type<
        scope<shared>, storage<std::shared_ptr<Class<>>>, interface<IClass>>();

    size_t count = 0;
    for (auto _ : state) {
        count +=
            is_empty(container.template resolve<std::shared_ptr<IClass>&>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void
resolve_container_shared_ptr_conversion_resolver(benchmark::State& state) {
    using namespace dingo;
    using container_type = container<ContainerTraits>;
    container_type container;
    container.template register_type<scope<shared>,
                                     storage<std::shared_ptr<Class<>>>,
                                     interface<Class<>, IClass>>();
    size_t count = 0;
    for (auto _ : state) {
        count +=
            is_empty(container.template resolve<std::shared_ptr<IClass>&>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits>
static void resolve_container_external(benchmark::State& state) {
    using namespace dingo;
    using container_type = dingo::container<ContainerTraits>;
    container_type container;
    int c = 1;
    container.template register_type<scope<external>, storage<int>>(c);

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(container.template resolve<int&>());
    }
    benchmark::DoNotOptimize(count);
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
        arena_allocator<char, 1024> allocator(arena);
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
        arena_allocator<char, 8192> allocator(arena);
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
                   dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_unique_int,
                   dingo::dynamic_container_traits)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_unique_string,
                   dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_unique_string,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared, dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_shared, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared_ptr,
                   dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_shared_ptr,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared_ptr_conversion_storage,
                   dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_shared_ptr_conversion_storage,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_shared_ptr_conversion_resolver,
                   dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_shared_ptr_conversion_resolver,
                   dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_external, dingo::static_container_traits<>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_external, dingo::dynamic_container_traits)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type,
                   dingo::container<dingo::static_container_traits<>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(register_type,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_arena,
                   dingo::container<dingo::dynamic_container_traits,
                                    dingo::arena_allocator<char, 1024>>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_10,
                   dingo::container<dingo::static_container_traits<>>)
    ->UseRealTime();
BENCHMARK_TEMPLATE(register_type_10,
                   dingo::container<dingo::dynamic_container_traits>)
    ->UseRealTime();

BENCHMARK_TEMPLATE(register_type_arena_10,
                   dingo::container<dingo::dynamic_container_traits,
                                    dingo::arena_allocator<char, 8192>>)
    ->UseRealTime();
} // namespace
