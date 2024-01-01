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
    static constexpr size_t size = 32; // TODO

  public:
    resolving_context()
        : resolve_counter_(), temporaries_size_(), resettables_size_(),
          constructibles_size_(), arena_allocator_(arena_) {}

    ~resolving_context() {
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

        // Destroy temporary instances
        for (size_t i = 0; i < temporaries_size_; ++i)
            temporaries_[i]->reset();
    }

    // TODO: this method seems useless but it is friend of a container and that
    // allows factories to not have to be known upfront.
    template <typename T, typename Container> T resolve(Container& container) {
        return container.template resolve<T, false>(*this);
    }

    void register_temporary(resettable_i* ptr) {
        // TODO: we are not registering temporaries, but resolvers
        // assert(std::find(temporaries_.begin(),
        //                 temporaries_.begin() + temporaries_size_,
        //                 ptr) == temporaries_.begin() + temporaries_size_);
        check_size(temporaries_size_);
        temporaries_[temporaries_size_++] = ptr;
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

    template <typename T> struct temporary : resettable_i {
        template <typename... Args>
        temporary(Args&&... args) : instance_(std::forward<Args>(args)...) {}
        void reset() override { instance_.~T(); }
        T* get() { return &instance_; }

      private:
        T instance_;
    };

    template <typename T, typename... Args> T& construct(Args&&... args) {
        if constexpr (std::is_trivially_destructible_v<T>) {
            auto allocator = allocator_traits::rebind<T>(arena_allocator_);
            auto instance = allocator_traits::allocate(allocator, 1);
            allocator_traits::construct(allocator, instance,
                                        std::forward<Args>(args)...);
            return *instance;
        } else {
            auto allocator =
                allocator_traits::rebind<temporary<T>>(arena_allocator_);
            auto instance = allocator_traits::allocate(allocator, 1);
            allocator_traits::construct(allocator, instance,
                                        std::forward<Args>(args)...);
            register_temporary(instance);
            return *instance->get();
        }
    }

    template <typename T, typename... Args>
    T& construct(resolving_context&, Args&&... args) {
        if constexpr (std::is_trivially_destructible_v<T>) {
            auto allocator = allocator_traits::rebind<T>(arena_allocator_);
            auto instance = allocator_traits::allocate(allocator, 1);
            allocator_traits::construct(allocator, instance,
                                        std::forward<Args>(args)...);
            return *instance;
        } else {
            auto allocator =
                allocator_traits::rebind<temporary<T>>(arena_allocator_);
            auto instance = allocator_traits::allocate(allocator, 1);
            allocator_traits::construct(allocator, instance,
                                        std::forward<Args>(args)...);
            register_temporary(instance);
            return *instance->get();
        }
    }

  private:
    void check_size(size_t count) {
        if (size == count)
            throw type_overflow_exception();
    }

    // TODO: this is fast, but non-generic, needs more work
    size_t resolve_counter_;

    size_t temporaries_size_;
    std::array<resettable_i*, size> temporaries_;

    size_t resettables_size_;
    std::array<resettable_i*, size> resettables_;

    size_t constructibles_size_;
    std::array<constructible_i*, size> constructibles_;

    // TODO: how big should this be?
    arena<512> arena_;
    arena_allocator<void, 512> arena_allocator_;
};

} // namespace dingo