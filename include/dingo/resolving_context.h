//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/aligned_storage.h>
#include <dingo/annotated.h>
#include <dingo/arena_allocator.h>
#include <dingo/exceptions.h>
#include <dingo/factory/constructor_detection.h>

#include <stack>
#include <vector>

namespace dingo {

class resolving_context {
  public:
    struct destructible {
        void* instance;
        void (*dtor)(void*);
    };

    struct closure {
        closure()
            : arena_(&arena_buffer_, DINGO_CONTEXT_ARENA_BUFFER_SIZE)
            , allocator_(arena_)
            , destructibles_(allocator_)
        {}

        aligned_storage_t<DINGO_CONTEXT_ARENA_BUFFER_SIZE, alignof(std::max_align_t)> arena_buffer_;
        arena<> arena_;
        arena_allocator<void> allocator_;
        std::vector<destructible, arena_allocator<destructible>> destructibles_;
    };

    resolving_context()
        : closures_(closure_.allocator_)
    {
        push(&closure_);
    }

    // TODO: motivation for latest changes was to get rid of this dtor
    // completely, as all types will be passed kind of directly. But as T is
    // deduced as const T&, as anything can bind to const T&, the value from
    // unique storage is passed as a const reference instead, meaning it has to
    // live somewhere across factory call and it needs to be destroyed after
    // resolve<> exits. Hence this class that unfortunatelly is quite
    // performance sensitive.
    ~resolving_context() {
        while(!closures_.empty()) {
            auto& c = closures_.back();
            for (auto it = c->destructibles_.rbegin(); it != c->destructibles_.rend(); ++it)
                it->dtor(it->instance);
            closures_.pop_back();
        }        
    }

    //    TODO: this method seems useless but it is friend of a container and
    //    that allows factories to not have to be known upfront.
    template <typename T, typename Container> T resolve(Container& container) {
        return container.template resolve<T, false>(*this);
    }

    template <typename T, typename... Args> T& construct(Args&&... args) {
        auto allocator = allocator_traits::rebind<T>(closures_.back()->allocator_);
        auto instance = allocator_traits::allocate(allocator, 1);
        allocator_traits::construct(allocator, instance,
                                    std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>)
            register_destructor(instance);
        return *instance;
    }

    template <typename T> T* allocate() {
        auto allocator = allocator_traits::rebind<T>(closures_.back()->allocator_);
        return allocator_traits::allocate(allocator, 1);
    }

    template <typename T, typename DetectionTag, typename Container> T construct_temporary(Container& container) {
        using Type = decay_t<T>;
        auto allocator = allocator_traits::rebind<Type>(closures_.back()->allocator_);
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

    void push(closure* c) {
        closures_.emplace_back(c);
    }

    void pop() {
        closures_.pop_back();
    }
    
  private:
    template <typename T> void register_destructor(T* instance) {
        static_assert(!std::is_trivially_destructible_v<T>);
        closures_.back()->destructibles_.push_back({instance, &destructor<T>});
    }

    template <typename T> static void destructor(void* ptr) {
        reinterpret_cast<T*>(ptr)->~T();
    }
    
    closure closure_;
    std::vector<closure*, arena_allocator<closure*>> closures_;
};

} // namespace dingo
