//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/memory/allocator.h>
#include <dingo/exceptions.h>
#include <dingo/type/type_list.h>

#include <tuple>
#include <type_traits>
#include <variant>

namespace dingo {
template <typename Arg, typename Definitions> struct index_tag;
template <typename Definitions, typename Value, typename Allocator>
struct index;

template <typename Key, typename Value, typename Allocator, typename Tag>
struct index_collection;

namespace detail {
template <typename Entry> struct index_entry;

template <typename Key, typename Tag> struct index_entry<type_list<Key, Tag>> {
    using key = Key;
    using tag = Tag;
};

template <typename Arg, typename Definitions> struct index_tag_impl;

template <typename Arg, typename Head, typename... Tail>
struct index_tag_impl<Arg, type_list<Head, Tail...>> {
    using type =
        std::conditional_t<std::is_same_v<typename index_entry<Head>::key, Arg>,
                           typename index_entry<Head>::tag,
                           typename index_tag_impl<Arg, type_list<Tail...>>::type>;
};

template <typename Arg> struct index_tag_impl<Arg, type_list<>> {
    using type = void;
};

template <typename Definitions, typename Value, typename Allocator>
struct index_impl;

template <typename Value, typename Allocator>
struct index_impl<type_list<>, Value, Allocator> {
    index_impl(Allocator&) {}

    template <typename Key> auto* try_get_index() { return nullptr; }
};

template <typename Value, typename Allocator, typename... Entries>
struct index_impl<type_list<Entries...>, Value, Allocator> {
    index_impl(Allocator&) {}

    template <typename T> struct index_ptr : allocator_base<Allocator> {
        // TODO: this pattern is already in class_factory_instance_ptr,
        // try to merge those
        index_ptr(Allocator& al) : allocator_base<Allocator>(al) {
            auto alloc = allocator_traits::rebind<T>(this->get_allocator());
            try {
                index_ = allocator_traits::allocate(alloc, 1);
                allocator_traits::construct(alloc, index_,
                                            this->get_allocator());
            } catch (...) {
                allocator_traits::deallocate(alloc, index_, 1);
                throw;
            }
        }

        ~index_ptr() {
            if (index_) {
                auto alloc = allocator_traits::rebind<T>(this->get_allocator());
                allocator_traits::destroy(alloc, index_);
                allocator_traits::deallocate(alloc, index_, 1);
            }
        }

        index_ptr() = default;
        index_ptr(const index_ptr<T>&) = delete;
        index_ptr(index_ptr<T>&& other)
            : allocator_base<Allocator>(std::move(other)) {
            std::swap(index_, other.index_);
        }

        T& operator*() {
            assert(index_);
            return *index_;
        }

      private:
        T* index_ = nullptr;
    };

    template <typename Key> auto& get_index(Allocator& allocator) {
        using index_type = index_collection<
            Key, Value, Allocator,
            typename index_tag_impl<Key, type_list<Entries...>>::type>;

        if (indexes_.index() == 0) {
            indexes_.template emplace<index_ptr<index_type>>(allocator);
        }

        return *std::get<index_ptr<index_type>>(indexes_);
    }

    template <typename Key> auto* try_get_index() {
        using index_type = index_collection<
            Key, Value, Allocator,
            typename index_tag_impl<Key, type_list<Entries...>>::type>;

        auto* index = std::get_if<index_ptr<index_type>>(&indexes_);
        return index ? &(**index) : nullptr;
    }

  private:
    std::variant<std::monostate,
                 index_ptr<index_collection<typename index_entry<Entries>::key,
                                            Value, Allocator,
                                            typename index_entry<Entries>::tag>>...>
        indexes_;
};
} // namespace detail

template <typename Arg, typename... Entries>
struct index_tag<Arg, std::tuple<Entries...>>
    : detail::index_tag_impl<Arg, type_list<to_type_list_t<Entries>...>> {};

template <typename Value, typename Allocator, typename... Entries>
struct index<std::tuple<Entries...>, Value, Allocator>
    : detail::index_impl<type_list<to_type_list_t<Entries>...>, Value,
                         Allocator> {
    using detail::index_impl<type_list<to_type_list_t<Entries>...>, Value,
                             Allocator>::index_impl;
};

} // namespace dingo
