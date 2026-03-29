//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <fruit/fruit.h>

#include <benchmark/benchmark.h>

namespace {
volatile int ClassCounter = 0;
template <size_t N> class Class {
  public:
    INJECT(Class()) { ClassCounter++; }
    int GetCounter() { return ClassCounter; }
};

fruit::Component<Class<0>> getUniqueComponent() {
    return fruit::createComponent();
}

static void resolve_fruit_unique(benchmark::State& state) {
    fruit::Injector<Class<0>> injector(getUniqueComponent);
    size_t counter = 0;
    for (auto _ : state) {
        auto cls = injector.get<Class<0>>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

fruit::Component<Class<0>> getSharedComponent() {
    static Class<0> cls;
    return fruit::createComponent().bindInstance(cls);
}

static void resolve_fruit_shared(benchmark::State& state) {
    fruit::Injector<Class<0>> injector(getSharedComponent);
    size_t counter = 0;
    for (auto _ : state) {
        auto& cls = injector.get<Class<0>&>();
        counter += cls.GetCounter();
    }
    benchmark::DoNotOptimize(counter);
    state.SetBytesProcessed(state.iterations());
}

BENCHMARK(resolve_fruit_unique);
BENCHMARK(resolve_fruit_shared);

} // namespace