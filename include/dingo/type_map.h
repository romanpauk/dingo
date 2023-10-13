#pragma once

#include <dingo/config.h>

#include <map>
#include <optional>
// #include <unordered_map>
#include <memory>

namespace dingo {
template <typename RTTI, typename Value, typename Allocator>
struct dynamic_type_map {
    dynamic_type_map(Allocator& allocator) : values_(allocator) {}

    template <typename Key> std::pair<Value&, bool> insert(Value&& value) {
        auto pb = values_.emplace(RTTI::template get_type_index<Key>(),
                                  std::forward<Value>(value));
        return {pb.first->second, pb.second};
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

  private:
    using map_allocator_type =
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            std::pair<const typename RTTI::type_index, Value>>;

    std::map<typename RTTI::type_index, Value,
             std::less<typename RTTI::type_index>, map_allocator_type>
        values_;
    // std::unordered_map< typename RTTI::type_index, Value, typename
    // RTTI::type_index::hasher, std::equal_to< typename RTTI::type_index >,
    // map_allocator_type > values_;
};

template <typename Tag, typename Value> struct static_type_map_node {
    std::optional<Value> value;
    static_type_map_node<Tag, Value>* next = nullptr;
#if !defined(NDEBUG)
    const void* owner;
#endif
};

template <typename Tag, typename Key, typename Value>
struct static_type_map_node_factory {
    static static_type_map_node<Tag, Value> node;
};

template <typename Tag, typename Key, typename Value>
static_type_map_node<Tag, Value>
    static_type_map_node_factory<Tag, Key, Value>::node;

template <typename RTTI, typename Tag, typename Value, typename Allocator>
struct static_type_map {
    template <typename Key>
    using node_factory = static_type_map_node_factory<Tag, Key, Value>;

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

    template <typename Key> std::pair<Value&, bool> insert(Value&& value) {
        auto& node = node_factory<Key>::node;
        if (!node.value) {
            assert(node.owner == nullptr);
#if !defined(NDEBUG)
            node.owner = this;
#endif
            node.value.emplace(std::forward<Value>(value));
            if (nodes_)
                node.next = nodes_;
            nodes_ = &node;
            ++size_;
            return {*node.value, true};
        }
        assert(node.owner == this);
        return {*node.value, false};
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

  private:
    static_type_map_node<Tag, Value>* nodes_;
    size_t size_;
};
} // namespace dingo
