//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <benchmark/benchmark.h>

#include <dingo/container.h>
#include <dingo/rtti/static_type_info.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>
#include <dingo/type_map.h>

#include <memory>
#include <optional>

struct basic_factory {
    virtual ~basic_factory() {}
    virtual void* resolve() = 0;
};

template <typename T> struct basic_unique_factory : basic_factory {
    void* resolve() override {
        new (&storage_) T;
        return &storage_;
    }

    void* resolve2() {
        new (&storage_) T;
        return &storage_;
    }

  private:
    std::aligned_storage_t<sizeof(T)> storage_;
};

template <typename T> struct basic_shared_factory : basic_factory {
    void* resolve() override {
        if (!storage_)
            storage_.emplace(T());
        return &*storage_;
    }

    void* resolve2() {
        if (!storage_)
            storage_.emplace(T());
        return &*storage_;
    }

  private:
    std::optional<T> storage_;
};

struct basic_unique_resolver {
    template <typename T> void register_type() {
        resolve_call_ = &resolve_call_impl<T>;
        factory_ = std::make_unique<T>();
    }

    template <typename T> T resolve() {
        if constexpr (std::is_trivially_destructible_v<T>) {
            return *static_cast<T*>(factory_->resolve());
        } else {
            T* p = static_cast<T*>(factory_->resolve());
            T value = std::move(*p);
            p->~T();
            return value;
        }
    }

    template <typename T> T resolve2() {
        if constexpr (std::is_trivially_destructible_v<T>) {
            return *static_cast<T*>((*resolve_call_)(factory_.get()));
        } else {
            T* p = static_cast<T*>((*resolve_call_)(factory_.get()));
            T value = std::move(*p);
            p->~T();
            return value;
        }
    }

    template <typename T> T resolve4() {
        void* tmp = cache_.get<T*>();
        if (tmp) {
            std::abort();
            return *static_cast<T*>(tmp);
        }

        if constexpr (std::is_trivially_destructible_v<T>) {
            return *static_cast<T*>((*resolve_call_)(factory_.get()));
        } else {
            T* p = static_cast<T*>((*resolve_call_)(factory_.get()));
            T value = std::move(*p);
            p->~T();
            return value;
        }
    }

    template <typename Factory> static void* resolve_call_impl(void* factory) {
        return reinterpret_cast<Factory*>(factory)->resolve2();
    }

  private:
    std::unique_ptr<basic_factory> factory_;
    void* (*resolve_call_)(void*);
    dingo::static_type_map<void*, void, std::allocator<void>> cache_;
};

struct basic_shared_resolver {
    template <typename T> void register_type() {
        resolve_call_ = &resolve_call_impl<T>;
        factory_ = std::make_unique<T>();
    }

    template <typename T> T resolve() {
        return *static_cast<T*>(factory_->resolve());
    }

    template <typename T> T resolve2() {
        return *static_cast<T*>((*resolve_call_)(factory_.get()));
    }

    template <typename T> T resolve3() {
        if (!cached_)
            cached_ = static_cast<T*>((*resolve_call_)(factory_.get()));

        return *static_cast<T*>(cached_);
    }

    template <typename T> T resolve4() {
        void* ptr = cache_.get<T*>();
        if (!ptr) {
            ptr = cache_
                      .insert<T*>(
                          static_cast<T*>((*resolve_call_)(factory_.get())))
                      .first;
        }

        return *static_cast<T*>(ptr);
    }

    // TODO: idea for container.h - non-unique results can avoid
    // virtual call with clever cache

    template <typename Factory> static void* resolve_call_impl(void* factory) {
        return reinterpret_cast<Factory*>(factory)->resolve2();
    }

  private:
    std::unique_ptr<basic_factory> factory_;
    void* (*resolve_call_)(void*);
    void* cached_ = nullptr;
    dingo::static_type_map<void*, void, std::allocator<void>> cache_;
};

bool is_empty(const std::string& val) { return val.empty(); }

bool is_empty(const int& val) { return val == 0; }

template <typename T> void basic_unique_resolve(benchmark::State& state) {
    basic_unique_resolver resolver;
    resolver.register_type<basic_unique_factory<T>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(resolver.resolve<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename T> void basic_unique_resolve2(benchmark::State& state) {
    basic_unique_resolver resolver;
    resolver.register_type<basic_unique_factory<T>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(resolver.resolve2<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename T> void basic_unique_resolve4(benchmark::State& state) {
    basic_unique_resolver resolver;
    resolver.register_type<basic_unique_factory<T>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(resolver.resolve4<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename T> void basic_shared_resolve(benchmark::State& state) {
    basic_shared_resolver resolver;
    resolver.register_type<basic_shared_factory<T>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(resolver.resolve<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename T> void basic_shared_resolve2(benchmark::State& state) {
    basic_shared_resolver resolver;
    resolver.register_type<basic_shared_factory<T>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(resolver.resolve2<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename T> void basic_shared_resolve3(benchmark::State& state) {
    basic_shared_resolver resolver;
    resolver.register_type<basic_shared_factory<T>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(resolver.resolve3<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename T> void basic_shared_resolve4(benchmark::State& state) {
    basic_shared_resolver resolver;
    resolver.register_type<basic_shared_factory<T>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(resolver.resolve4<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename Container, typename T>
static void container_unique_resolve(benchmark::State& state) {
    using namespace dingo;
    using container_type = Container;
    container_type container;
    container.template register_type<scope<unique>, storage<T>,
                                     factory<constructor<T()>>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(container.template resolve<T>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

template <typename Container, typename T>
static void container_shared_resolve(benchmark::State& state) {
    using namespace dingo;
    using container_type = Container;
    container_type container;
    container.template register_type<scope<shared>, storage<T>,
                                     factory<constructor<T()>>>();

    size_t count = 0;
    for (auto _ : state) {
        count += is_empty(container.template resolve<T&>());
    }
    benchmark::DoNotOptimize(count);
    state.SetBytesProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(basic_unique_resolve, int)->UseRealTime();
BENCHMARK_TEMPLATE(basic_unique_resolve, std::string)->UseRealTime();

BENCHMARK_TEMPLATE(basic_unique_resolve2, int)->UseRealTime();
BENCHMARK_TEMPLATE(basic_unique_resolve2, std::string)->UseRealTime();

BENCHMARK_TEMPLATE(basic_unique_resolve4, int)->UseRealTime();
BENCHMARK_TEMPLATE(basic_unique_resolve4, std::string)->UseRealTime();

BENCHMARK_TEMPLATE(basic_shared_resolve, int)->UseRealTime();
BENCHMARK_TEMPLATE(basic_shared_resolve, std::string)->UseRealTime();

BENCHMARK_TEMPLATE(basic_shared_resolve2, int)->UseRealTime();
BENCHMARK_TEMPLATE(basic_shared_resolve2, std::string)->UseRealTime();

BENCHMARK_TEMPLATE(basic_shared_resolve3, int)->UseRealTime();
BENCHMARK_TEMPLATE(basic_shared_resolve3, std::string)->UseRealTime();

BENCHMARK_TEMPLATE(basic_shared_resolve4, int)->UseRealTime();
BENCHMARK_TEMPLATE(basic_shared_resolve4, std::string)->UseRealTime();

template <bool Cache, typename Tag = void>
struct container_traits : dingo::static_container_traits<Tag> {
    static constexpr bool cache_enabled = Cache;
    template <typename TagT> using rebind_t = container_traits<Cache, TagT>;
    using tag_type = Tag;
};

BENCHMARK_TEMPLATE(container_unique_resolve,
                   dingo::container<container_traits<true>>, int)
    ->UseRealTime();
BENCHMARK_TEMPLATE(container_unique_resolve,
                   dingo::container<container_traits<false>>, int)
    ->UseRealTime();
BENCHMARK_TEMPLATE(container_unique_resolve,
                   dingo::container<container_traits<true>>, std::string)
    ->UseRealTime();
BENCHMARK_TEMPLATE(container_unique_resolve,
                   dingo::container<container_traits<false>>, std::string)
    ->UseRealTime();

BENCHMARK_TEMPLATE(container_shared_resolve,
                   dingo::container<container_traits<true>>, int)
    ->UseRealTime();
BENCHMARK_TEMPLATE(container_shared_resolve,
                   dingo::container<container_traits<false>>, int)
    ->UseRealTime();
BENCHMARK_TEMPLATE(container_shared_resolve,
                   dingo::container<container_traits<true>>, std::string)
    ->UseRealTime();
BENCHMARK_TEMPLATE(container_shared_resolve,
                   dingo::container<container_traits<false>>, std::string)
    ->UseRealTime();
