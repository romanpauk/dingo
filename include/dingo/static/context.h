//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/context_base.h>
#include <dingo/static/graph.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <new>

namespace dingo {

template <typename StaticRegistry, bool PartialDependencies>
class basic_static_context;

namespace detail {

template <typename StaticRegistry, bool PartialDependencies>
struct static_context_closure_selector;

template <typename StaticRegistry>
struct static_context_closure_selector<StaticRegistry, false> {
    using execution_traits = detail::basic_static_execution_traits<
        StaticRegistry, false>;
    using type = detail::static_context_closure<
        execution_traits::max_destructible_slots,
        execution_traits::max_temporary_slots,
        execution_traits::max_temporary_size == 0
            ? 1
            : execution_traits::max_temporary_size,
        execution_traits::max_temporary_align == 0
            ? alignof(std::max_align_t)
            : execution_traits::max_temporary_align>;
};

template <typename StaticRegistry>
struct static_context_closure_selector<StaticRegistry, true> {
    using execution_traits = detail::basic_static_execution_traits<
        StaticRegistry, true>;
    using type = detail::fixed_context_closure<
        execution_traits::max_destructible_slots,
        execution_traits::max_temporary_slots,
        execution_traits::max_temporary_size == 0
            ? 1
            : execution_traits::max_temporary_size,
        execution_traits::max_temporary_align == 0
            ? alignof(std::max_align_t)
            : execution_traits::max_temporary_align>;
};

template <typename StaticRegistry>
using binding_context = basic_static_context<StaticRegistry, true>;

} // namespace detail

template <typename StaticRegistry, bool PartialDependencies = false>
class basic_static_context : public detail::context_path_state {
    using execution_traits =
        detail::basic_static_execution_traits<StaticRegistry,
                                             PartialDependencies>;
    static constexpr std::size_t closure_capacity_ =
        execution_traits::max_preserved_closure_depth + 1;
    static constexpr std::size_t destructible_capacity_ =
        execution_traits::max_destructible_slots;
    static constexpr std::size_t temporary_slot_capacity_ =
        execution_traits::max_temporary_slots;
    static constexpr std::size_t temporary_slot_size_ =
        execution_traits::max_temporary_size == 0
            ? 1
            : execution_traits::max_temporary_size;
    static constexpr std::size_t temporary_slot_align_ =
        execution_traits::max_temporary_align == 0
            ? alignof(std::max_align_t)
            : execution_traits::max_temporary_align;
    using closure_type = typename detail::static_context_closure_selector<
        StaticRegistry, PartialDependencies>::type;

  public:
    basic_static_context() { closures_[0] = &closure_; }

    ~basic_static_context() {
        while (closure_count_ != 0) {
            closures_[--closure_count_]->reset();
        }
    }

    template <typename T, typename Container>
    T resolve(Container& container) {
        if constexpr (is_keyed_v<T>) {
            using request_type = keyed_type_t<T>;
            using key_type = keyed_key_t<T>;
            return T(container.template resolve<request_type, false>(
                *this, key<key_type>{}));
        } else {
            return container.template resolve<T, false>(*this);
        }
    }

    template <typename T, typename DetectionTag, typename Container>
    T construct_temporary(Container& container) {
        using temporary_type = normalized_type_t<T>;

        auto* instance = allocate_temporary_storage<temporary_type>();
        detail::default_constructor_detection<temporary_type, DetectionTag>()
            .template construct<temporary_type>(instance, *this, container);
        if constexpr (!std::is_trivially_destructible_v<temporary_type>) {
            register_destructor(instance);
        }

        if constexpr (std::is_lvalue_reference_v<T>) {
            return *instance;
        } else {
            return std::move(*instance);
        }
    }

    template <typename T, typename... Args>
    T& construct(Args&&... args) {
        auto* instance = allocate_temporary_storage<T>();
        new (instance) T(std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            register_destructor(instance);
        }
        return *instance;
    }

    template <typename T>
    T* allocate() {
        return allocate_temporary_storage<T>();
    }

    void push(closure_type* closure) {
        assert(!contains(closure));
        assert(closure_count_ < closures_.size());
        closures_[closure_count_++] = closure;
    }

    void pop() {
        assert(closure_count_ != 0);
        --closure_count_;
    }

    bool contains(const closure_type* candidate) const {
        for (std::size_t index = 0; index < closure_count_; ++index) {
            if (closures_[index] == candidate) {
                return true;
            }
        }
        return false;
    }

  private:
    template <typename T>
    T* allocate_temporary_storage() {
        static_assert(sizeof(T) <= temporary_slot_size_,
                      "static_context temporary size must fit the compile-time "
                      "temporary slot bound");
        static_assert(alignof(T) <= temporary_slot_align_,
                      "static_context temporary alignment must fit the "
                      "compile-time temporary slot bound");
        static_assert(temporary_slot_capacity_ != 0,
                      "static_context requires at least one compile-time "
                      "temporary slot for this resolution path");

        auto* fixed = active_closure().template try_allocate_temporary<T>();
        assert(fixed != nullptr);
        return fixed;
    }

    closure_type& active_closure() {
        assert(closure_count_ != 0);
        return *closures_[closure_count_ - 1];
    }

    template <typename T>
    void register_destructor(T* instance) {
        static_assert(!std::is_trivially_destructible_v<T>);
        active_closure().add_destructor(instance, &destructor<T>);
    }

    template <typename T>
    static void destructor(void* ptr) {
        reinterpret_cast<T*>(ptr)->~T();
    }

    std::array<closure_type*, closure_capacity_> closures_{};
    std::size_t closure_count_ = 1;
    closure_type closure_;
};

template <typename StaticRegistry>
using static_context = basic_static_context<StaticRegistry, false>;

} // namespace dingo
