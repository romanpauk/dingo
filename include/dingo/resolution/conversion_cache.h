//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/memory/aligned_storage.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>
#include <dingo/type/type_list.h>

#include <variant>

namespace dingo {
namespace detail {
template <typename T, typename... Args>
void construct_class_instance(void* ptr, Args&&... args) {
    new (ptr) T(std::forward<Args>(args)...);
}

template <typename T, typename Source>
std::enable_if_t<type_traits<T>::enabled &&
                 type_traits<std::remove_reference_t<Source>>::enabled>
construct_class_instance(void* ptr, Source&& source) {
    new (ptr) T(type_conversion_traits<
                T, std::remove_reference_t<Source>>::convert(
        std::forward<Source>(source)));
}
} // namespace detail

template <typename... Types> struct conversion_cache;

template <> struct conversion_cache<> {
    void reset() {}
};

template <typename T> struct conversion_cache_entry {
    template <typename... Args> T& construct(Args&&... args) {
        auto* instance = reinterpret_cast<T*>(&storage_);
        if (initialized_) {
            return *instance;
        }

        detail::construct_class_instance<T>(instance,
                                            std::forward<Args>(args)...);
        initialized_ = true;
        return *instance;
    }

    void reset() {
        if (initialized_) {
            if constexpr (!std::is_trivially_destructible_v<T>)
                reinterpret_cast<T*>(&storage_)->~T();
            initialized_ = false;
        }
    }

    aligned_storage_t<sizeof(T), alignof(T)> storage_;
    bool initialized_ = false;
};

template <typename... Types> struct conversion_cache
    : conversion_cache_entry<Types>...
{
    ~conversion_cache() {
        reset();
    }

    template <typename T, typename... Args> T& construct(Args&&... args) {
        return static_cast<conversion_cache_entry<T>&>(*this).construct(
            std::forward<Args>(args)...);
    }

    void reset() {
        (static_cast<conversion_cache_entry<Types>&>(*this).reset(), ...);
    }
};

// TODO: the idea was that conversions will act as a cache of pointers
// context-owned instances. That would require pushing and popping of
// the context on each resolution. Right now, conversions are like
// "expanded variant".
template <typename... Args> struct conversion_cache<type_list<Args...>>
    : conversion_cache<Args...>
{}; 

template <> struct conversion_cache<type_list<>> {
    void reset() {}
};

} // namespace dingo
