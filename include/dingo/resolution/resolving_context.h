//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/memory/allocator.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/registration/annotated.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/resolution/resolving_frame_fwd.h>
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

        ~closure() { reset(); }

        void reset() {
            if (!destructibles_.empty()) {
                for (auto it = destructibles_.rbegin(); it != destructibles_.rend(); ++it)
                    it->dtor(it->instance);
            }
            destructibles_.clear();
            // `destructibles_` stores its capacity inside `arena_`. Rebuild the
            // vector before rewinding the arena so the vector destructor never
            // observes capacity backed by invalidated storage.
            using destructibles_type = decltype(destructibles_);
            destructibles_.~destructibles_type();
            arena_.reset();
            new (&destructibles_) destructibles_type(arena_);
        }

        aligned_storage_t<DINGO_CLOSURE_ARENA_BUFFER_SIZE, alignof(std::max_align_t)> arena_buffer_;
        arena<> arena_;

        struct destructible {
            void* instance;
            void (*dtor)(void*);
        };

        std::vector<destructible, arena_allocator<destructible>> destructibles_;
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

    template <typename T> detail::resolving_frame track_type();

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
        // A closure represents one owner of preserved temporaries. Recursive
        // shared resolution must be stopped by the type guard before the same
        // factory tries to reactivate its closure while it is already active.
        assert(!contains(c));
        closures_.emplace_back(c);
    }

    void pop() {
        closures_.pop_back();
    }

    std::size_t closures_size() const { return closures_.size(); }

    bool contains(const closure* candidate) const {
        for (auto* active : closures_) {
            if (active == candidate) {
                return true;
            }
        }
        return false;
    }

    bool has_type_path() const;

    const type_descriptor* active_type() const;

    const type_descriptor* parent_type() const;

    void append_type_path(std::string& message) const;

  private:
    friend class detail::resolving_frame;

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
    detail::resolving_frame* active_resolving_frame_ = nullptr;
    closure closure_;
};

} // namespace dingo

#include <dingo/resolution/resolving_frame.h>

namespace dingo {

namespace detail {

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
inline resolving_frame::resolving_frame(resolving_context& context,
                                        type_descriptor type)
    : context_(&context)
    , parent_(context.active_resolving_frame_)
    , type_(type) {
    // The frame object itself is the linked stack node for the active
    // resolution path, so entering the scope just makes this the new head.
    // GCC can warn here when the frame lives inside a stack RAII guard even
    // though the destructor restores parent_ before that guard leaves scope.
    context_->active_resolving_frame_ = this;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

inline resolving_frame::~resolving_frame() {
    if (context_) {
        // Frames unwind strictly LIFO; restoring parent_ pops this node from
        // the active type path without touching any arena-backed state.
        assert(context_->active_resolving_frame_ == this);
        context_->active_resolving_frame_ = parent_;
    }
}

} // namespace detail

inline bool resolving_context::has_type_path() const {
    return active_resolving_frame_ != nullptr;
}

inline const type_descriptor* resolving_context::active_type() const {
    return active_resolving_frame_ != nullptr
               ? &active_resolving_frame_->type_
               : nullptr;
}

inline const type_descriptor* resolving_context::parent_type() const {
    return active_resolving_frame_ != nullptr &&
                   active_resolving_frame_->parent_ != nullptr
               ? &active_resolving_frame_->parent_->type_
               : nullptr;
}

inline void resolving_context::append_type_path(std::string& message) const {
    std::vector<type_descriptor> names;
    for (auto* frame = active_resolving_frame_; frame != nullptr;
         frame = frame->parent_) {
        names.emplace_back(frame->type_);
    }

    for (auto it = names.rbegin(); it != names.rend(); ++it) {
        if (it != names.rbegin()) {
            message += " -> ";
        }
        append_type_name(message, *it);
    }
}

template <typename T>
detail::resolving_frame resolving_context::track_type() {
    return detail::resolving_frame(*this, describe_type<T>());
}

} // namespace dingo
