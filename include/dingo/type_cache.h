//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <map>
#include <optional>
// #include <unordered_map>
#include <memory>

namespace dingo {
template <typename Value, typename RTTI, typename Allocator>
struct dynamic_type_cache {
    dynamic_type_cache(Allocator& allocator) : values_(allocator) {}

    template <typename Key, typename ValueT>
    void insert(ValueT&& value) {
        auto pb = values_.emplace(RTTI::template get_type_index<Key>(),
                                  std::forward<ValueT>(value));
        (void)pb;
        assert(pb.second);
    }

    template <typename Key> const Value& get() {
        auto it = values_.find(RTTI::template get_type_index<Key>());
        if (it != values_.end()) return it->second;
        return empty_;
    }

  private:
    using allocator_type =
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            std::pair<const typename RTTI::type_index, Value>>;

    std::map<typename RTTI::type_index, Value,
             std::less<typename RTTI::type_index>, allocator_type>
        values_;
    // std::unordered_map< typename RTTI::type_index, Value, typename
    // RTTI::type_index::hasher, std::equal_to< typename RTTI::type_index >,
    // map_allocator_type > values_;

    static Value empty_;
};

template <typename Value, typename RTTI, typename Allocator>
Value dynamic_type_cache<Value, RTTI, Allocator>::empty_;

template <typename Value, typename Tag> struct static_type_cache_node {
    Value value;
    static_type_cache_node<Value, Tag>* next = nullptr;
#if !defined(NDEBUG)
    const void* owner;
#endif
};

template <typename Key, typename Value, typename Tag>
struct static_type_cache_node_factory {
    static static_type_cache_node<Value, Tag> node;
};

template <typename Key, typename Value, typename Tag>
static_type_cache_node<Value, Tag>
    static_type_cache_node_factory<Key, Value, Tag>::node;

template <typename Value, typename Tag,
          typename Allocator = std::allocator<void>>
struct static_type_cache {
    template <typename Key>
    using node_factory = static_type_cache_node_factory<Key, Value, Tag>;
    using node_type = static_type_cache_node<Value, Tag>;

    static_type_cache() = default;
    static_type_cache(Allocator&) {}

    ~static_type_cache() {
        auto node = nodes_;
        while (node) {
            assert(node->value ? node->owner == this : node->owner == nullptr);
            node->value = Value();
#if !defined(NDEBUG)
            node->owner = nullptr;
#endif
            node = node->next;
        }
    }

    template <typename Key, typename ValueT> void insert(ValueT&& value) {
        auto& node = node_factory<Key>::node;
        assert(!node.value);
        assert(node.owner == nullptr);
        node.value = std::forward<ValueT>(value);
        node.next = nodes_;
        nodes_ = &node;
        ++size_;
#if !defined(NDEBUG)
        node.owner = this;
#endif
    }

    template <typename Key> const Value& get() {
        auto& node = node_factory<Key>::node;
        return node.value;
    }

  private:
    node_type* nodes_ = nullptr;
    size_t size_ = 0;
};
} // namespace dingo
