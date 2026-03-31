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
#include <dingo/exceptions.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/resolution/constructor_format.h>
#include <dingo/type/type_list.h>

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

    struct constructor_frame {
        const constructor_frame* parent;
        void (*append)(std::string&);
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

    template <typename Invocation, typename Fn>
    decltype(auto) invoke_constructor(Fn&& fn) {
        struct constructor_trace_guard {
            explicit constructor_trace_guard(resolving_context& context)
                : context_(&context)
                , parent_(context.active_constructor_frame_)
                , frame_{parent_, &detail::append_constructor_invocation<Invocation>} {
                context_->active_constructor_frame_ = &frame_;
            }

            ~constructor_trace_guard() {
                context_->active_constructor_frame_ = parent_;
            }

            resolving_context* context_;
            const constructor_frame* parent_;
            constructor_frame frame_;
        } trace(*this);

        try {
            if constexpr (std::is_void_v<decltype(fn())>) {
                fn();
            } else {
                return fn();
            }
        } catch (const type_not_found_exception& e) {
            rethrow_with_constructor_path(e);
        } catch (const type_not_convertible_exception& e) {
            rethrow_with_constructor_path(e);
        } catch (const type_ambiguous_exception& e) {
            rethrow_with_constructor_path(e);
        } catch (const type_recursion_exception& e) {
            rethrow_with_constructor_path(e);
        } catch (const type_not_constructed_exception& e) {
            rethrow_with_constructor_path(e);
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

    bool has_constructor_path() const {
        return active_constructor_frame_ != nullptr;
    }

    void append_constructor_path(std::string& message) const {
        std::vector<const constructor_frame*> frames;
        for (auto* frame = active_constructor_frame_; frame != nullptr;
             frame = frame->parent) {
            frames.emplace_back(frame);
        }

        for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
            if (it != frames.rbegin()) {
                message += " -> ";
            }
            (*it)->append(message);
        }
    }

  private:
    template <typename Exception>
    [[noreturn]] void rethrow_with_constructor_path(const Exception& e) const {
        std::string message = e.what();
        if (has_constructor_path() &&
            message.find("constructor path: ") == std::string::npos) {
            message += "; constructor path: ";
            append_constructor_path(message);
        }
        throw Exception(std::move(message));
    }

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
    const constructor_frame* active_constructor_frame_ = nullptr;
    closure closure_;

    template <typename T, bool DefaultConstructible>
    friend struct class_recursion_guard;
};

} // namespace dingo
