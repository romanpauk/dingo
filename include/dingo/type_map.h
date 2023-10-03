#pragma once

#include <dingo/config.h>

#include <map>
#include <optional>
// #include <unordered_map>
#include <memory>

namespace dingo {
template <typename RTTI, typename Value, typename Allocator>
struct dynamic_type_map {
    dynamic_type_map(Allocator allocator) : values_(allocator) {}

    template <typename Key> std::pair<Value&, bool> insert(Value&& value) {
        auto pb = values_.emplace(RTTI::template get_type_index<Key>(),
                                  std::forward<Value>(value));
        return {pb.first->second, pb.second};
    }

    template <typename Key> Value* at() {
        auto it = values_.find(RTTI::template get_type_index<Key>());
        return it != values_.end() ? &it->second : nullptr;
    }

    size_t size() const { return values_.size(); }

    Value& front() { return values_.begin()->second; }

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

template <typename Tag, typename Value> struct static_type_map_node_data {
    std::optional<Value> value;
    static_type_map_node_data<Tag, Value>* next = nullptr;
};

template <typename Tag, typename Key, typename Value>
struct static_type_map_node {
    static static_type_map_node_data<Tag, Value> data;
};

template <typename Tag, typename Key, typename Value>
static_type_map_node_data<Tag, Value>
    static_type_map_node<Tag, Key, Value>::data;

template <typename RTTI, typename Tag, typename Value, typename Allocator>
struct static_type_map {
    template <typename Key>
    using node_type = static_type_map_node<Tag, Key, Value>;

    static_type_map(Allocator&) : nodes_(), size_() {}

    ~static_type_map() {
        auto node = nodes_;
        while (node) {
            node->value.reset();
            node = node->next;
        }
    }

    template <typename Key> std::pair<Value&, bool> insert(Value&& value) {
        auto& val = node_type<Key>::data.value;
        if (!val) {
            val.emplace(std::forward<Value>(value));
            if (nodes_)
                node_type<Key>::data.next = nodes_;
            nodes_ = &node_type<Key>::data;
            ++size_;
            return {*val, true};
        }
        return {*val, false};
    }

    template <typename Key> Value* at() {
        auto& value = node_type<Key>::data.value;
        return value ? &*value : nullptr;
    }

    size_t size() const { return size_; }

    Value& front() { return *nodes_->value; }

  private:
    static_type_map_node_data<Tag, Value>* nodes_;
    size_t size_;
};
} // namespace dingo
