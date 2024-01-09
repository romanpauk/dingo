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
#include <dingo/constructible_i.h>
#include <dingo/exceptions.h>
#include <dingo/resettable_i.h>

#include <algorithm>
#include <array>
#include <deque>
#include <forward_list>
#include <list>

namespace dingo {

class resolving_context {
    static constexpr size_t size = DINGO_CONTEXT_SIZE;

  public:
    resolving_context()
        : resolve_counter_(), resettables_size_(), constructibles_size_(),
          destructors_size_(), arena_allocator_(arena_) {}

    ~resolving_context() {
        // TODO: rethink exception support as this is expensive
        // TODO: constructibles_ are used only when there is cyclical storage.
        // Use trait?
        if (resolve_counter_ == 0) {
            if (constructibles_size_) {
                // Note that invocation of construct(_, 0) can grow
                // constructibles_.
                for (size_t i = 0; i < constructibles_size_; ++i)
                    constructibles_[i]->construct();
            }
        } else {
            // Rollback changes in container
            for (size_t i = 0; i < resettables_size_; ++i)
                resettables_[i]->reset();
        }

        for (size_t i = 0; i < destructors_size_; ++i)
            destructors_[i].dtor(destructors_[i].instance);
    }

    // TODO: this method seems useless but it is friend of a container and that
    // allows factories to not have to be known upfront.
    template <typename T, typename Container> T resolve(Container& container) {
        return container.template resolve<T, false>(*this);
    }

    void register_resettable(resettable_i* ptr) {
        check_size(resettables_size_);
        resettables_[resettables_size_++] = ptr;
    }

    void register_constructible(constructible_i* ptr) {
        check_size(constructibles_size_);
        constructibles_[constructibles_size_++] = ptr;
    }

    // Use this counter to determine if exception was thrown or not
    // (std::uncaught_exceptions is slow).
    void increment() { ++resolve_counter_; }
    void decrement() { --resolve_counter_; }

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

    template <typename T> void register_destructor(T* instance) {
        static_assert(!std::is_trivially_destructible_v<T>);
        check_size(destructors_size_);
        destructors_[destructors_size_++] = {instance, &destructor<T>};
    }

  private:
    template <typename T> static void destructor(void* ptr) {
        reinterpret_cast<T*>(ptr)->~T();
    }

    void check_size(size_t count) {
        if (size == count)
            throw type_overflow_exception();
    }

    // TODO: this is fast, but non-generic, needs more work
    uint16_t resolve_counter_;
    uint16_t resettables_size_;
    uint16_t constructibles_size_;
    uint16_t destructors_size_;

    struct destructible {
        void* instance;
        void (*dtor)(void*);
    };

    std::array<resettable_i*, size> resettables_;
    std::array<constructible_i*, size> constructibles_;
    std::array<destructible, size> destructors_;

    // TODO: how big should this be?
    arena<512> arena_;
    arena_allocator<void, 512> arena_allocator_;
};

} // namespace dingo