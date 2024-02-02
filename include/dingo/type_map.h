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
struct dynamic_type_map {
    dynamic_type_map(Allocator& allocator) : values_(allocator) {}

    template <typename Key> std::pair<Value&, bool> insert(Value&& value) {
        auto pb = values_.emplace(RTTI::template get_type_index<Key>(),
                                  std::forward<Value>(value));
        return {pb.first->second, pb.second};
    }

    template <typename Key> bool erase() {
        return values_.erase(RTTI::template get_type_index<Key>());
    }

    template <typename Key> Value* get() {
        auto it = values_.find(RTTI::template get_type_index<Key>());
        return it != values_.end() ? &it->second : nullptr;
    }

    size_t size() const { return values_.size(); }

    Value& front() {
        assert(!values_.empty());
        return values_.begin()->second;
    }

    auto begin() { return values_.begin(); }
    auto end() { return values_.end(); }

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
};

template <typename Value, typename Tag> struct static_type_map_node {
    std::optional<Value> value;
    static_type_map_node<Value, Tag>* next = nullptr;
#if !defined(NDEBUG)
    const void* owner;
#endif
};

template <typename Key, typename Value, typename Tag>
struct static_type_map_node_factory {
    static static_type_map_node<Value, Tag> node;
};

template <typename Key, typename Value, typename Tag>
static_type_map_node<Value, Tag>
    static_type_map_node_factory<Key, Value, Tag>::node;

template <typename Value, typename Tag,
          typename Allocator = std::allocator<void>>
struct static_type_map {
    template <typename Key>
    using node_factory = static_type_map_node_factory<Key, Value, Tag>;
    using node_type = static_type_map_node<Value, Tag>;

    static_type_map() : nodes_(), size_() {}
    static_type_map(Allocator&) : nodes_(), size_() {}

    ~static_type_map() {
        auto node = nodes_;
        while (node) {
            assert(node->value ? node->owner == this : node->owner == nullptr);
            node->value.reset();
#if !defined(NDEBUG)
            node->owner = nullptr;
#endif
            node = node->next;
        }
    }

    template <typename Key, typename... Args>
    std::pair<Value&, bool> insert(Args&&... args) {
        auto& node = node_factory<Key>::node;
        if (!node.value) {
            assert(node.owner == nullptr);
            node.value.emplace(std::forward<Args>(args)...);
            node.next = nodes_;
            nodes_ = &node;
            ++size_;
#if !defined(NDEBUG)
            node.owner = this;
#endif
            return {*node.value, true};
        }
        assert(node.owner == this);
        return {*node.value, false};
    }

    template <typename Key> bool erase() {
        auto& node = node_factory<Key>::node;
        if (!node.value)
            return false;
        node.value.reset();
#if !defined(NDEBUG)
        node.owner = nullptr;
#endif
        return true;
    }

    template <typename Key> Value* get() {
        auto& node = node_factory<Key>::node;
        assert(node.value ? node.owner == this : node.owner == nullptr);
        return node.value ? &*node.value : nullptr;
    }

    size_t size() const { return size_; }

    Value& front() {
        assert(nodes_);
        return *nodes_->value;
    }

    struct iterator {
        iterator(node_type* node) : node_(node) {}

        iterator& operator++() {
            assert(node_);
            node_ = node_->next;
            return *this;
        }

        iterator operator++(int) {
            assert(node_);
            auto n = node_;
            node_ = node_->next;
            return n;
        }

        bool operator==(iterator other) const { return node_ == other.node_; }
        bool operator!=(iterator other) const { return node_ != other.node_; }

        std::pair<void*, Value&> operator*() {
            assert(node_);
            return {nullptr, *node_->value};
        }

      private:
        node_type* node_;
    };

    iterator begin() { return nodes_; }

    iterator end() { return nullptr; }

  private:
    node_type* nodes_;
    size_t size_;
};
} // namespace dingo
