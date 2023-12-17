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

#include <array>
#include <deque>
#include <forward_list>
#include <list>

namespace dingo {

class resolving_context {
    static constexpr size_t size = 32; // TODO

  public:
    resolving_context()
        : resolve_counter_(), class_instances_size_(), resettables_size_(),
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
        for (size_t i = 0; i < class_instances_size_; ++i)
            class_instances_[i]->reset();
    }

    // TODO: this method seems useless but it is friend of a container and that
    // allows factories to not have to be known upfront.
    template <typename T, typename Container> T resolve(Container& container) {
        return container.template resolve<T, false>(*this);
    }

    void register_class_instance(resettable_i* ptr) {
        check_size(class_instances_size_);
        class_instances_[class_instances_size_++] = ptr;
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

    template <typename T> struct arena_instance : resettable_i {
        template <typename... Args>
        arena_instance(arena_allocator<arena_instance<T>>& allocator,
                       Args&&... args)
            : allocator_(allocator), instance_(std::forward<Args>(args)...) {}

        void reset() override {
            arena_allocator<arena_instance<T>> allocator = allocator_;
            allocator_traits::destroy(allocator, this);
            allocator_traits::deallocate(allocator, this, 1);
        }

        T* get() { return &instance_; }

      private:
        arena_allocator<arena_instance<T>> allocator_;
        T instance_;
    };

    template <typename T, typename... Args> T* construct(Args&&... args) {
        auto allocator =
            allocator_traits::rebind<arena_instance<T>>(arena_allocator_);
        auto ptr = allocator_traits::allocate(allocator, 1);
        allocator_traits::construct(allocator, ptr, allocator,
                                    std::forward<Args>(args)...);
        register_class_instance(ptr);
        return ptr->get();
    }

  private:
    void check_size(size_t count) {
        if (size == count)
            throw type_overflow_exception();
    }

    // TODO: this is fast, but non-generic, needs more work
    size_t resolve_counter_;

    size_t class_instances_size_;
    std::array<resettable_i*, size> class_instances_;

    size_t resettables_size_;
    std::array<resettable_i*, size> resettables_;

    size_t constructibles_size_;
    std::array<constructible_i*, size> constructibles_;

    // TODO: this is a hack for storage<unique> conversions
    arena<128> arena_;
    arena_allocator<char> arena_allocator_;
};

} // namespace dingo