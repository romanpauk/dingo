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

template <typename ContainerTraits> static void resolve_container_unique(benchmark::State& state) {
    using container_type = dingo::container<ContainerTraits>;
    container_type container;
    container.template register_binding<dingo::storage<dingo::unique, Class<0>>>();

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

template <typename ContainerTraits> static void resolve_container_shared(benchmark::State& state) {
    using container_type = dingo::container<ContainerTraits>;
    container_type container;
    container.template register_binding<dingo::storage<dingo::shared, Class<0>>>();

    int counter = 0;
    for (auto _ : state) {
        auto& cls = container.template resolve<Class<0>&>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits> static void resolve_container_external(benchmark::State& state) {
    using container_type = dingo::container<ContainerTraits>;
    container_type container;
    Class<0> c;
    container.template register_binding<dingo::storage<dingo::external, Class<0>>>(c);

    int counter = 0;
    for (auto _ : state) {
        auto& cls = container.template resolve<Class<0>&>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

template <typename ContainerTraits> static void construct_baseline(benchmark::State& state) {
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

BENCHMARK(resolve_baseline_unique);
BENCHMARK_TEMPLATE(resolve_container_unique, dingo::static_container_traits<>)->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_unique, dingo::dynamic_container_traits)->UseRealTime();

BENCHMARK(resolve_baseline_shared);
BENCHMARK_TEMPLATE(resolve_container_shared, dingo::static_container_traits<>)->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_shared, dingo::dynamic_container_traits)->UseRealTime();

BENCHMARK_TEMPLATE(resolve_container_external, dingo::static_container_traits<>)->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_external, dingo::dynamic_container_traits)->UseRealTime();

BENCHMARK_TEMPLATE(construct_baseline, dingo::static_container_traits<>)->UseRealTime();
BENCHMARK_TEMPLATE(construct_baseline, dingo::dynamic_container_traits)->UseRealTime();

} // namespace
