//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/allocator.h>
#include <dingo/aligned_storage.h>
#include <dingo/annotated.h>
#include <dingo/arena_allocator.h>
#include <dingo/exceptions.h>
#include <dingo/factory/constructor_detection.h>

#include <vector>

namespace dingo {

class resolving_context {
  public:   
    struct closure {
        closure()
            : arena_(arena_buffer_)
            , destructibles_(arena_)
        {}

        void reset() {
            if (!destructibles_.empty()) {
                for (auto it = destructibles_.rbegin(); it != destructibles_.rend(); ++it)
                    it->dtor(it->instance);
                destructibles_.clear();
            }
            arena_.reset();
        }

        aligned_storage_t<DINGO_CLOSURE_ARENA_BUFFER_SIZE, alignof(std::max_align_t)> arena_buffer_;
        arena<> arena_;

        struct destructible {
            void* instance;
            void (*dtor)(void*);
        };

        std::vector<destructible, arena_allocator<destructible>> destructibles_;
    };

    struct type_frame {
        const type_frame* parent;
        type_descriptor type;
    };

    resolving_context()
        : arena_(arena_buffer_)
        , closures_(arena_)
    {
        push(&closure_);
    }

    ~resolving_context() {
        for (auto it = closures_.rbegin(); it != closures_.rend(); ++it)
            (*it)->reset();
    }

    template <typename T, typename Container> T resolve(Container& container) {
        return container.template resolve<T, false>(*this);
    }

    template <typename T, typename... Args> T& construct(Args&&... args) {
        arena_allocator<void> alloc(closures_.back()->arena_);
        auto allocator = allocator_traits::rebind<T>(alloc);
        auto instance = allocator_traits::allocate(allocator, 1);
        allocator_traits::construct(allocator, instance,
                                    std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>)
            register_destructor(instance);
        return *instance;
    }

    template <typename T> T* allocate() {
        arena_allocator<void> alloc(closures_.back()->arena_);
        auto allocator = allocator_traits::rebind<T>(alloc);
        return allocator_traits::allocate(allocator, 1);
    }

    template <typename T, typename DetectionTag, typename Container>
    T construct_temporary(Container& container) {
        using temporary_type = normalized_type_t<T>;

        auto& active_closure = *closures_.back();
        arena_allocator<void> alloc(active_closure.arena_);
        auto allocator = allocator_traits::rebind<temporary_type>(alloc);
        auto instance = allocator_traits::allocate(allocator, 1);
        constructor_detection<temporary_type, DetectionTag>()
            .template construct<temporary_type>(instance, *this, container);
        if constexpr (!std::is_trivially_destructible_v<temporary_type>)
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

    std::size_t closures_size() const { return closures_.size(); }

    bool has_type_path() const { return active_type_frame_ != nullptr; }

    void append_type_path(std::string& message) const {
        std::vector<type_descriptor> names;
        for (auto* frame = active_type_frame_; frame != nullptr;
             frame = frame->parent) {
            names.emplace_back(frame->type);
        }

        for (auto it = names.rbegin(); it != names.rend(); ++it) {
            if (it != names.rbegin()) {
                message += " -> ";
            }
            append_type_name(message, *it);
        }
    }

  private:
    template <typename T> void register_destructor(T* instance) {
        static_assert(!std::is_trivially_destructible_v<T>);
        closures_.back()->destructibles_.push_back({instance, &destructor<T>});
    }

    template <typename T> static void destructor(void* ptr) {
        reinterpret_cast<T*>(ptr)->~T();
    }

    aligned_storage_t<DINGO_CONTEXT_ARENA_BUFFER_SIZE, alignof(std::max_align_t)> arena_buffer_;
    arena<> arena_;
    std::vector<closure*, arena_allocator<closure*>> closures_;
    const type_frame* active_type_frame_ = nullptr;
    closure closure_;

    template <typename T, bool DefaultConstructible>
    friend struct class_recursion_guard;
};

} // namespace dingo
