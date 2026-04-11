//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <benchmark/benchmark.h>

#include <dingo/resolution/type_cache.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/type/type_map.h>

#include <cstdint>
#include <cstdlib>

namespace {
template <std::size_t I> struct table_key {};

using rtti_type = dingo::rtti<dingo::typeid_provider>;
using allocator_type = std::allocator<char>;

template <typename Fn> void dispatch_table_size(int size, Fn&& fn) {
    switch (size) {
    case 1:
        fn(std::integral_constant<std::size_t, 1>{});
        return;
    case 2:
        fn(std::integral_constant<std::size_t, 2>{});
        return;
    case 4:
        fn(std::integral_constant<std::size_t, 4>{});
        return;
    case 8:
        fn(std::integral_constant<std::size_t, 8>{});
        return;
    case 16:
        fn(std::integral_constant<std::size_t, 16>{});
        return;
    case 32:
        fn(std::integral_constant<std::size_t, 32>{});
        return;
    case 64:
        fn(std::integral_constant<std::size_t, 64>{});
        return;
    case 128:
        fn(std::integral_constant<std::size_t, 128>{});
        return;
    case 256:
        fn(std::integral_constant<std::size_t, 256>{});
        return;
    case 512:
        fn(std::integral_constant<std::size_t, 512>{});
        return;
    case 1024:
        fn(std::integral_constant<std::size_t, 1024>{});
        return;
    default:
        std::abort();
    }
}

template <typename Table, std::size_t... I>
void fill_type_map_impl(Table& table, std::index_sequence<I...>) {
    (benchmark::DoNotOptimize(
         table.template insert<table_key<I>>(static_cast<int>(I))),
     ...);
}

template <typename Table, std::size_t N> void fill_type_map(Table& table) {
    fill_type_map_impl(table, std::make_index_sequence<N>{});
}

template <typename Cache, std::size_t... I>
void fill_type_cache_impl(Cache& cache, std::index_sequence<I...>) {
    (cache.template insert<table_key<I>>(static_cast<std::uintptr_t>(I + 1)),
     ...);
}

template <typename Cache, std::size_t N> void fill_type_cache(Cache& cache) {
    fill_type_cache_impl(cache, std::make_index_sequence<N>{});
}

void apply_table_sizes(benchmark::internal::Benchmark* benchmark) {
    for (int size : {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024}) {
        benchmark->Arg(size);
    }
}

template <template <typename, typename, typename> class Table>
static void type_map_insert(benchmark::State& state) {
    for (auto _ : state) {
        allocator_type allocator;
        Table<int, rtti_type, allocator_type> table(allocator);
        dispatch_table_size(state.range(0), [&](auto size_tag) {
            fill_type_map<decltype(table), decltype(size_tag)::value>(table);
        });
        benchmark::DoNotOptimize(table);
    }

    state.SetItemsProcessed(state.iterations() * state.range(0));
}

template <template <typename, typename, typename> class Table>
static void type_map_get_hit(benchmark::State& state) {
    allocator_type allocator;
    Table<int, rtti_type, allocator_type> table(allocator);
    dispatch_table_size(state.range(0), [&](auto size_tag) {
        fill_type_map<decltype(table), decltype(size_tag)::value>(table);
    });

    for (auto _ : state) {
        dispatch_table_size(state.range(0), [&](auto size_tag) {
            auto* value = table.template get<
                table_key<decltype(size_tag)::value - 1>>();
            benchmark::DoNotOptimize(value);
        });
    }

    state.SetItemsProcessed(state.iterations());
}

template <template <typename, typename, typename> class Cache>
static void type_cache_get_hit(benchmark::State& state) {
    allocator_type allocator;
    Cache<std::uintptr_t, rtti_type, allocator_type> cache(allocator);
    dispatch_table_size(state.range(0), [&](auto size_tag) {
        fill_type_cache<decltype(cache), decltype(size_tag)::value>(cache);
    });

    for (auto _ : state) {
        dispatch_table_size(state.range(0), [&](auto size_tag) {
            auto value = cache.template get<
                table_key<decltype(size_tag)::value - 1>>();
            benchmark::DoNotOptimize(value);
        });
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(type_map_insert, dingo::map_type_map)
    ->Apply(apply_table_sizes)
    ->UseRealTime();
BENCHMARK_TEMPLATE(type_map_insert, dingo::dynamic_type_map)
    ->Apply(apply_table_sizes)
    ->UseRealTime();

BENCHMARK_TEMPLATE(type_map_get_hit, dingo::map_type_map)
    ->Apply(apply_table_sizes)
    ->UseRealTime();
BENCHMARK_TEMPLATE(type_map_get_hit, dingo::dynamic_type_map)
    ->Apply(apply_table_sizes)
    ->UseRealTime();

BENCHMARK_TEMPLATE(type_cache_get_hit, dingo::map_type_cache)
    ->Apply(apply_table_sizes)
    ->UseRealTime();
BENCHMARK_TEMPLATE(type_cache_get_hit, dingo::dynamic_type_cache)
    ->Apply(apply_table_sizes)
    ->UseRealTime();
} // namespace
