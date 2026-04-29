//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/exceptions.h>
#include <dingo/core/keyed.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/memory/aligned_storage.h>
#include <dingo/memory/allocator.h>
#include <dingo/memory/arena_allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/resolution/resolving_frame_fwd.h>

#include <array>
#include <cassert>
#include <vector>

namespace dingo {
namespace detail {

struct context_destructible {
    void* instance;
    void (*dtor)(void*);
};

struct context_closure_base {
    virtual ~context_closure_base() = default;
    virtual void reset() = 0;
    virtual arena<>& arena_storage() = 0;
    virtual void add_destructor(void* instance, void (*dtor)(void*)) = 0;
};

struct context_closure : context_closure_base {
    context_closure()
        : arena_(arena_buffer_)
        , destructibles_(arena_) {}

    ~context_closure() { reset(); }

    context_closure(const context_closure&) = delete;
    context_closure& operator=(const context_closure&) = delete;

    void reset() override {
        if (!destructibles_.empty()) {
            for (auto it = destructibles_.rbegin(); it != destructibles_.rend();
                 ++it) {
                it->dtor(it->instance);
            }
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

    aligned_storage_t<DINGO_CLOSURE_ARENA_BUFFER_SIZE,
                      alignof(std::max_align_t)>
        arena_buffer_;
    arena<> arena_;
    std::vector<context_destructible, arena_allocator<context_destructible>>
        destructibles_;

    arena<>& arena_storage() override { return arena_; }

    void add_destructor(void* instance, void (*dtor)(void*)) override {
        destructibles_.push_back({instance, dtor});
    }
};

template <std::size_t DestructibleCapacity,
          std::size_t TemporarySlotCapacity = 0,
          std::size_t TemporarySlotSize = 1,
          std::size_t TemporarySlotAlign = alignof(std::max_align_t)>
struct static_context_closure {
    static constexpr std::size_t destructible_capacity_ = DestructibleCapacity;
    static constexpr std::size_t temporary_slot_capacity_ =
        TemporarySlotCapacity;
    static constexpr std::size_t temporary_storage_capacity_ =
        temporary_slot_capacity_ == 0 ? 1 : temporary_slot_capacity_;

    static_context_closure() = default;
    ~static_context_closure() { reset(); }

    static_context_closure(const static_context_closure&) = delete;
    static_context_closure& operator=(const static_context_closure&) = delete;

    void reset() {
        while (destructible_count_ != 0) {
            auto& destructible = destructibles_[--destructible_count_];
            destructible.dtor(destructible.instance);
        }
        temporary_count_ = 0;
    }

    void add_destructor(void* instance, void (*dtor)(void*)) {
        assert(destructible_count_ < destructible_capacity_);
        destructibles_[destructible_count_++] = {instance, dtor};
    }

    template <typename T>
    T* try_allocate_temporary() {
        if constexpr (temporary_slot_capacity_ == 0) {
            return nullptr;
        } else if constexpr (sizeof(T) > TemporarySlotSize ||
                             alignof(T) > TemporarySlotAlign) {
            return nullptr;
        } else {
            if (temporary_count_ >= temporary_slot_capacity_) {
                return nullptr;
            }
            return reinterpret_cast<T*>(&temporary_slots_[temporary_count_++]);
        }
    }

    std::array<context_destructible, destructible_capacity_> destructibles_{};
    std::size_t destructible_count_ = 0;
    std::array<aligned_storage_t<TemporarySlotSize, TemporarySlotAlign>,
               temporary_storage_capacity_>
        temporary_slots_{};
    std::size_t temporary_count_ = 0;
};

template <std::size_t DestructibleCapacity, std::size_t TemporarySlotCapacity = 0,
          std::size_t TemporarySlotSize = 1,
          std::size_t TemporarySlotAlign = alignof(std::max_align_t)>
struct fixed_context_closure : context_closure_base {
    static constexpr std::size_t destructible_capacity_ = DestructibleCapacity;
    static constexpr std::size_t temporary_slot_capacity_ =
        TemporarySlotCapacity;
    static constexpr std::size_t temporary_storage_capacity_ =
        temporary_slot_capacity_ == 0 ? 1 : temporary_slot_capacity_;

    fixed_context_closure()
        : arena_(arena_buffer_) {}

    ~fixed_context_closure() { reset(); }

    fixed_context_closure(const fixed_context_closure&) = delete;
    fixed_context_closure& operator=(const fixed_context_closure&) = delete;

    void reset() override {
        while (destructible_count_ != 0) {
            auto& destructible = destructibles_[--destructible_count_];
            destructible.dtor(destructible.instance);
        }
        temporary_count_ = 0;
        arena_.reset();
    }

    arena<>& arena_storage() override { return arena_; }

    void add_destructor(void* instance, void (*dtor)(void*)) override {
        assert(destructible_count_ < destructible_capacity_);
        destructibles_[destructible_count_++] = {instance, dtor};
    }

    template <typename T>
    T* try_allocate_temporary() {
        if constexpr (temporary_slot_capacity_ == 0) {
            return nullptr;
        } else if constexpr (sizeof(T) > TemporarySlotSize ||
                             alignof(T) > TemporarySlotAlign) {
            return nullptr;
        } else {
            if (temporary_count_ >= temporary_slot_capacity_) {
                return nullptr;
            }
            return reinterpret_cast<T*>(&temporary_slots_[temporary_count_++]);
        }
    }

    aligned_storage_t<DINGO_CLOSURE_ARENA_BUFFER_SIZE,
                      alignof(std::max_align_t)>
        arena_buffer_;
    arena<> arena_;
    std::array<context_destructible, destructible_capacity_> destructibles_{};
    std::size_t destructible_count_ = 0;
    std::array<aligned_storage_t<TemporarySlotSize, TemporarySlotAlign>,
               temporary_storage_capacity_>
        temporary_slots_{};
    std::size_t temporary_count_ = 0;
};

class context_path_state {
  public:
    template <typename T>
    detail::resolving_frame track_type();

    bool has_type_path() const;

    const type_descriptor* active_type() const;

    const type_descriptor* parent_type() const;

    void append_type_path(std::string& message) const;

  protected:
    friend class resolving_frame;

    detail::resolving_frame* active_resolving_frame_ = nullptr;
};

class context_state : public context_path_state {
  public:
    context_state()
        : arena_(arena_buffer_)
        , closures_(arena_) {
        closures_.emplace_back(&closure_);
    }

    ~context_state() {
        for (auto it = closures_.rbegin(); it != closures_.rend(); ++it) {
            (*it)->reset();
        }
    }

    template <typename T, typename... Args>
    T& construct(Args&&... args) {
        arena_allocator<void> alloc(closures_.back()->arena_storage());
        auto allocator = allocator_traits::rebind<T>(alloc);
        auto instance = allocator_traits::allocate(allocator, 1);
        allocator_traits::construct(allocator, instance,
                                    std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            register_destructor(instance);
        }
        return *instance;
    }

    template <typename T>
    T* allocate() {
        arena_allocator<void> alloc(closures_.back()->arena_storage());
        auto allocator = allocator_traits::rebind<T>(alloc);
        return allocator_traits::allocate(allocator, 1);
    }

    void push(context_closure_base* c) {
        // A closure represents one owner of preserved temporaries. Recursive
        // shared resolution must be stopped by the type guard before the same
        // factory tries to reactivate its closure while it is already active.
        assert(!contains(c));
        closures_.emplace_back(c);
    }

    void pop() { closures_.pop_back(); }

    bool contains(const context_closure_base* candidate) const {
        for (auto* active : closures_) {
            if (active == candidate) {
                return true;
            }
        }
        return false;
    }

  protected:
    template <typename T>
    void register_destructor(T* instance) {
        static_assert(!std::is_trivially_destructible_v<T>);
        closures_.back()->add_destructor(instance, &destructor<T>);
    }

    template <typename T>
    static void destructor(void* ptr) {
        reinterpret_cast<T*>(ptr)->~T();
    }

    aligned_storage_t<DINGO_CONTEXT_ARENA_BUFFER_SIZE,
                      alignof(std::max_align_t)>
        arena_buffer_;
    arena<> arena_;
    std::vector<context_closure_base*, arena_allocator<context_closure_base*>>
        closures_;
    context_closure closure_;
};

} // namespace detail
} // namespace dingo

#include <dingo/resolution/resolving_frame.h>

namespace dingo {
namespace detail {

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#endif
inline resolving_frame::resolving_frame(context_path_state& context,
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

inline bool context_path_state::has_type_path() const {
    return active_resolving_frame_ != nullptr;
}

inline const type_descriptor* context_path_state::active_type() const {
    return active_resolving_frame_ != nullptr
               ? &active_resolving_frame_->type_
               : nullptr;
}

inline const type_descriptor* context_path_state::parent_type() const {
    return active_resolving_frame_ != nullptr &&
                   active_resolving_frame_->parent_ != nullptr
               ? &active_resolving_frame_->parent_->type_
               : nullptr;
}

inline void context_path_state::append_type_path(std::string& message) const {
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
detail::resolving_frame context_path_state::track_type() {
    return detail::resolving_frame(*this, describe_type<T>());
}

} // namespace detail
} // namespace dingo
