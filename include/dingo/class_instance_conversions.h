//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/aligned_storage.h>
#include <dingo/arena_allocator.h>
#include <dingo/decay.h>
#include <dingo/resettable_i.h>
#include <dingo/type_list.h>

#include <variant>

namespace dingo {

template< typename T > struct class_instance_conversion {
    template< typename... Args > T& construct(Args&&... args) {
        auto* instance = reinterpret_cast<T*>(&storage_);
        if (initialized_) {
            return *instance;
        }

        new (instance) T(std::forward<Args>(args)...);
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

template< typename... Types > struct class_instance_conversions
    : class_instance_conversion< Types >...
{
    ~class_instance_conversions() {
        reset();
    }

    template < typename T, typename... Args > T& construct(Args&&... args) {
        return static_cast<class_instance_conversion<T>&>(*this).construct(std::forward<Args>(args)...);
    }

    void reset() {
        (static_cast<class_instance_conversion<Types>&>(*this).reset(), ...);
    }
};

// TODO: the idea was that conversions will act as a cache of pointers
// context-owned instances. That would require pushing and popping of
// the context on each resolution. Right now, conversions are like
// "expanded variant".
template< typename... Args > struct class_instance_conversions< type_list<Args...> >
: class_instance_conversions<Args...>
{}; 

} // namespace dingo
