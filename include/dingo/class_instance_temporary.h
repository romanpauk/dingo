//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/arena_allocator.h>
#include <dingo/decay.h>
#include <dingo/resettable_i.h>
#include <dingo/type_list.h>

#include <variant>

namespace dingo {

template <typename T,
          bool IsTriviallyDestructible = std::is_trivially_destructible_v<T>>
struct temporary;

template <typename T> struct temporary<T, false> : resettable_i {
    template <typename... Args>
    temporary(Args&&... args) : instance_(std::forward<Args>(args)...) {}
    void reset() override { instance_.~T(); }
    T* get() { return &instance_; }

  private:
    T instance_;
};

template <typename T> struct temporary<T, true> {
  private:
    // This exists only for size calculation
    std::aligned_storage_t<sizeof(T)> instance_;
};

template <typename> struct class_instance_temporary_storage {};
template <typename... Args>
struct class_instance_temporary_storage<type_list<Args...>> {
    using type = std::aligned_union_t<0, void*, temporary<Args>...>;
    static constexpr std::size_t size = sizeof(type);
    static constexpr std::size_t alignment =
        alignof(type); // type::alignment_value;
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

        static_assert(sizeof(temporary<T>) <= sizeof(storage_));
        assert(!resettable_);

        if constexpr (std::is_trivially_destructible_v<T>) {
            auto* instance = reinterpret_cast<T*>(&storage_);
            new (instance) T(std::forward<Args>(args)...);
            ptr_ = &instance;
#if !defined(NDEBUG)
            type_ = RTTI::template get_type_index<T*>();
#endif
            return instance;
        } else {

            auto* instance = reinterpret_cast<temporary<T>*>(&storage_);
            new (instance) temporary<T>(std::forward<Args>(args)...);
            ptr_ = instance->get();
            resettable_ = instance;
#if !defined(NDEBUG)
            type_ = RTTI::template get_type_index<T*>();
#endif
            return *instance->get();
        }
    }

    void reset() {
        if (resettable_) {
            resettable_->reset();
            resettable_ = nullptr;
        }
        ptr_ = nullptr;
    }

  private:
    std::aligned_storage_t<
        class_instance_temporary_storage<ConversionTypes>::size>
        storage_;
    void* ptr_ = 0;
    resettable_i* resettable_ = 0;
#if !defined(NDEBUG)
    typename RTTI::type_index type_;
#endif
};

template <typename RTTI> struct class_instance_temporary<RTTI, type_list<>> {
    template <typename T, typename... Args> T& construct(Args&&...);
    void reset() {}
};
} // namespace dingo
