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

template <typename> struct class_instance_temporary_storage {};
template <typename... Args>
struct class_instance_temporary_storage<type_list<Args...>> {
    using type = dingo::aligned_union_t<0, void*, Args...>;
};

template <typename RTTI, typename ConversionTypes>
struct class_instance_temporary {
#if !defined(NDEBUG)
    class_instance_temporary() : type_(RTTI::template get_type_index<void>()) {}
#endif

    ~class_instance_temporary() { reset(); }

    template <typename T, typename... Args> T& construct(Args&&... args) {
        if (ptr_) {
            assert(type_ == RTTI::template get_type_index<T*>());
            return *reinterpret_cast<T*>(ptr_);
        }

        static_assert(sizeof(T) <= sizeof(storage_));
        assert(!ptr_);
        auto* instance = reinterpret_cast<T*>(&storage_);
        new (instance) T(std::forward<Args>(args)...);
        ptr_ = instance;
#if !defined(NDEBUG)
        type_ = RTTI::template get_type_index<T*>();
#endif
        if constexpr (!std::is_trivially_destructible_v<T>)
            destructor_ = &destructor<T>;
        return *instance;
    }

    void reset() {
        if (destructor_) {
            (*destructor_)(ptr_);
            destructor_ = nullptr;
        }
        ptr_ = nullptr;
    }

  private:
    template <typename T> static void destructor(void* ptr) {
        reinterpret_cast<T*>(ptr)->~T();
    }

    typename class_instance_temporary_storage<ConversionTypes>::type storage_;
    void* ptr_ = nullptr;
    void (*destructor_)(void*) = nullptr;
#if !defined(NDEBUG)
    typename RTTI::type_index type_;
#endif
};

template <typename RTTI> struct class_instance_temporary<RTTI, type_list<>> {
    template <typename T, typename... Args> T& construct(Args&&...);
    void reset() {}
};
} // namespace dingo
