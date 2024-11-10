//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/annotated.h>
#include <dingo/arena_allocator.h>
#include <dingo/exceptions.h>
#include <dingo/factory/constructor_detection.h>

#include <array>

namespace dingo {

class resolving_context {
    static constexpr size_t size = DINGO_CONTEXT_SIZE;

  public:
    resolving_context() : destructors_size_(), arena_allocator_(arena_) {}

    // TODO: motivation for latest changes was to get rid of this dtor
    // completely, as all types will be passed kind of directly. But as T is
    // deduced as const T&, as anything can bind to const T&, the value from
    // unique storage is passed as a const reference instead, meaning it has to
    // live somewhere across factory call and it needs to be destroyed after
    // resolve<> exits. Hence this class that unfortunatelly is quite
    // performance sensitive.
    ~resolving_context() {
        if (destructors_size_) {
            do {
                --destructors_size_;
                destructors_[destructors_size_].dtor(
                    destructors_[destructors_size_].instance);
            } while (destructors_size_ > 0);
        }
    }

    //    TODO: this method seems useless but it is friend of a container and
    //    that allows factories to not have to be known upfront.
    template <typename T, typename Container> T resolve(Container& container) {
        return container.template resolve<T, false>(*this);
    }

    template <typename T, typename... Args> T& construct(Args&&... args) {
        auto allocator = allocator_traits::rebind<T>(arena_allocator_);
        auto instance = allocator_traits::allocate(allocator, 1);
        allocator_traits::construct(allocator, instance,
                                    std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>)
            register_destructor(instance);
        return *instance;
    }

    template <typename T> T* allocate() {
        auto allocator = allocator_traits::rebind<T>(arena_allocator_);
        return allocator_traits::allocate(allocator, 1);
    }

    template <typename T, typename DetectionTag, typename Container> T construct_temporary(Container& container) {
        using Type = decay_t<T>;
        auto allocator = allocator_traits::rebind<Type>(arena_allocator_);
        auto instance = allocator_traits::allocate(allocator, 1);
        constructor_detection<Type, DetectionTag>().template construct<Type>(instance, *this, container);
        if constexpr (!std::is_trivially_destructible_v<Type>)
            register_destructor(instance);
        if constexpr (std::is_lvalue_reference_v<T>) {
            return *instance;
        } else {
            return std::move(*instance);
        }
    }

  private:
    template <typename T> void register_destructor(T* instance) {
        static_assert(!std::is_trivially_destructible_v<T>);
        check_size(destructors_size_);
        destructors_[destructors_size_++] = {instance, &destructor<T>};
    }

    template <typename T> static void destructor(void* ptr) {
        reinterpret_cast<T*>(ptr)->~T();
    }

    void check_size(size_t count) {
        if (size == count)
            throw type_context_overflow_exception();
    }

    // TODO: this is fast, but non-generic, needs more work
    size_t destructors_size_;

    struct destructible {
        void* instance;
        void (*dtor)(void*);
    };

    std::array<destructible, size> destructors_;

    // TODO: how big should this be?
    arena<32 * size> arena_;
    arena_allocator<void, 32 * size> arena_allocator_;
};

} // namespace dingo
