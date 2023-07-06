#include <dingo/container.h>
#include <dingo/storage/unique.h>
#include <dingo/storage/shared.h>

#include <benchmark/benchmark.h>

BENCHMARK_MAIN();

int ClassCounter = 0;
template< size_t N > class Class {
public:
    Class() {
        ClassCounter += 1;
    }

    int GetCounter() { return ClassCounter; }
};

static void resolve_baseline_unique(benchmark::State& state) {
    for (auto _ : state) {
        Class<0> cls;
        benchmark::DoNotOptimize(cls.GetCounter());
    }

    state.SetBytesProcessed(state.iterations());
}

template< typename ContainerTraits > static void resolve_container_unique(benchmark::State& state) 
{
    using container_type = dingo::container< ContainerTraits >;
    container_type container;
    container.template register_binding< dingo::storage< container_type, dingo::unique, Class<0> > >();
    
    for (auto _ : state) {
        auto cls = container.template resolve<Class<0>>();
        benchmark::DoNotOptimize(cls.GetCounter());
    }
   
    state.SetBytesProcessed(state.iterations());
}

static void resolve_baseline_shared(benchmark::State& state) {
    Class<0> cls;
    for (auto _ : state) {
        benchmark::DoNotOptimize(cls.GetCounter());
    }

    state.SetBytesProcessed(state.iterations());
}

template< typename ContainerTraits > static void resolve_container_shared(benchmark::State& state) 
{
    using container_type = dingo::container< ContainerTraits >;
    container_type container;
    container.template register_binding< dingo::storage< container_type, dingo::shared, Class<0> > >();
    
    for (auto _ : state) {
        auto& cls = container.template resolve<Class<0>&>();
        benchmark::DoNotOptimize(cls.GetCounter());
    }
   
    state.SetBytesProcessed(state.iterations());
}

BENCHMARK(resolve_baseline_unique);
BENCHMARK_TEMPLATE(resolve_container_unique, dingo::static_container_traits<>)->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_unique, dingo::dynamic_container_traits)->UseRealTime();

BENCHMARK(resolve_baseline_shared);
BENCHMARK_TEMPLATE(resolve_container_shared, dingo::static_container_traits<>)->UseRealTime();
BENCHMARK_TEMPLATE(resolve_container_shared, dingo::dynamic_container_traits)->UseRealTime();
