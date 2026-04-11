//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <algorithm>
#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename Key, typename Value, typename Allocator,
          typename Compare = std::less<Key>>
struct sorted_vector_table {
    using value_type = std::pair<Key, Value>;
    using allocator_type =
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            value_type>;
    using allocator_traits = std::allocator_traits<allocator_type>;
    using iterator = value_type*;
    using const_iterator = const value_type*;

    explicit sorted_vector_table(Allocator& allocator) : allocator_(allocator) {}

    sorted_vector_table(const sorted_vector_table&) = delete;
    sorted_vector_table& operator=(const sorted_vector_table&) = delete;

    sorted_vector_table(sorted_vector_table&& other) noexcept
        : allocator_(std::move(other.allocator_)),
          values_(other.values_),
          size_(other.size_),
          capacity_(other.capacity_),
          compare_(std::move(other.compare_)) {
        other.values_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    sorted_vector_table& operator=(sorted_vector_table&& other) noexcept {
        if (this != &other) {
            clear();
            deallocate();
            allocator_ = std::move(other.allocator_);
            values_ = other.values_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            compare_ = std::move(other.compare_);
            other.values_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    ~sorted_vector_table() {
        clear();
        deallocate();
    }

    template <typename ValueT>
    std::pair<Value&, bool> insert(const Key& key, ValueT&& value) {
        auto it = lower_bound(key);
        if (it != end() && keys_equal(it->first, key)) {
            return {it->second, false};
        }
        const auto index = static_cast<size_t>(it - begin());

        if (size_ == capacity_) {
            rebuild_and_insert(index, key, std::forward<ValueT>(value),
                               next_capacity());
        } else if constexpr (std::is_nothrow_move_constructible_v<value_type>) {
            insert_in_place(index, key, std::forward<ValueT>(value));
        } else {
            rebuild_and_insert(index, key, std::forward<ValueT>(value),
                               capacity_);
        }

        return {values_[index].second, true};
    }

    bool erase(const Key& key) {
        auto it = lower_bound(key);
        if (it == end() || !keys_equal(it->first, key)) {
            return false;
        }
        const auto index = static_cast<size_t>(it - begin());
        if constexpr (std::is_nothrow_move_constructible_v<value_type>) {
            erase_at(index);
        } else if (size_ == 1) {
            allocator_traits::destroy(allocator_, values_);
            size_ = 0;
        } else {
            rebuild_without(index, capacity_);
        }
        return true;
    }

    Value* get(const Key& key) {
        auto it = lower_bound(key);
        if (it == end() || !keys_equal(it->first, key)) {
            return nullptr;
        }
        return &it->second;
    }

    size_t size() const { return size_; }

    Value& front() {
        assert(size_ > 0);
        return values_[0].second;
    }

    iterator begin() { return values_; }
    iterator end() { return size_ == 0 ? values_ : values_ + size_; }

  private:
    iterator lower_bound(const Key& key) {
        if (size_ == 0) {
            return values_;
        }
        return std::lower_bound(
            begin(), end(), key,
            [&](const value_type& entry, const Key& candidate) {
                return compare_(entry.first, candidate);
            });
    }

    bool keys_equal(const Key& lhs, const Key& rhs) const {
        return !compare_(lhs, rhs) && !compare_(rhs, lhs);
    }

    void clear() {
        if (size_ == 0) {
            return;
        }
        destroy_range(values_, values_ + size_);
        size_ = 0;
    }

    void destroy_range(iterator first, iterator last) {
        while (first != last) {
            allocator_traits::destroy(allocator_, first);
            ++first;
        }
    }

    void deallocate() {
        if (!values_) {
            return;
        }
        allocator_traits::deallocate(allocator_, values_, capacity_);
        values_ = nullptr;
        capacity_ = 0;
    }

    size_t next_capacity() const {
        return capacity_ == 0 ? 4 : capacity_ * 2;
    }

    template <typename ValueT>
    void rebuild_and_insert(size_t index, const Key& key, ValueT&& value,
                            size_t new_capacity) {
        auto* next = allocator_traits::allocate(allocator_, new_capacity);
        size_t constructed = 0;

        try {
            for (; constructed < index; ++constructed) {
                allocator_traits::construct(
                    allocator_, next + constructed, values_[constructed].first,
                    std::move_if_noexcept(values_[constructed].second));
            }

            allocator_traits::construct(allocator_, next + constructed, key,
                                        std::forward<ValueT>(value));
            ++constructed;

            for (size_t source = index; source < size_; ++source, ++constructed) {
                allocator_traits::construct(
                    allocator_, next + constructed, values_[source].first,
                    std::move_if_noexcept(values_[source].second));
            }
        } catch (...) {
            destroy_range(next, next + constructed);
            if (next) {
                allocator_traits::deallocate(allocator_, next, new_capacity);
            }
            throw;
        }

        clear();
        deallocate();
        values_ = next;
        size_ = constructed;
        capacity_ = new_capacity;
    }

    template <typename ValueT>
    void insert_in_place(size_t index, const Key& key, ValueT&& value) {
        if (index == size_) {
            allocator_traits::construct(allocator_, values_ + size_, key,
                                        std::forward<ValueT>(value));
            ++size_;
            return;
        }

        allocator_traits::construct(allocator_, values_ + size_,
                                    values_[size_ - 1].first,
                                    std::move(values_[size_ - 1].second));

        for (size_t current = size_ - 1; current > index; --current) {
            allocator_traits::destroy(allocator_, values_ + current);
            allocator_traits::construct(allocator_, values_ + current,
                                        values_[current - 1].first,
                                        std::move(values_[current - 1].second));
        }

        allocator_traits::destroy(allocator_, values_ + index);
        allocator_traits::construct(allocator_, values_ + index, key,
                                    std::forward<ValueT>(value));
        ++size_;
    }

    void erase_at(size_t index) {
        allocator_traits::destroy(allocator_, values_ + index);
        for (size_t current = index; current + 1 < size_; ++current) {
            allocator_traits::construct(allocator_, values_ + current,
                                        values_[current + 1].first,
                                        std::move(values_[current + 1].second));
            allocator_traits::destroy(allocator_, values_ + current + 1);
        }
        --size_;
    }

    void rebuild_without(size_t index, size_t new_capacity) {
        auto* next = allocator_traits::allocate(allocator_, new_capacity);
        size_t constructed = 0;

        try {
            for (size_t source = 0; source < size_; ++source) {
                if (source == index) {
                    continue;
                }
                allocator_traits::construct(
                    allocator_, next + constructed, values_[source].first,
                    std::move_if_noexcept(values_[source].second));
                ++constructed;
            }
        } catch (...) {
            destroy_range(next, next + constructed);
            if (next) {
                allocator_traits::deallocate(allocator_, next, new_capacity);
            }
            throw;
        }

        clear();
        deallocate();
        values_ = next;
        size_ = constructed;
        capacity_ = new_capacity;
    }

    allocator_type allocator_;
    value_type* values_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
    Compare compare_;
};
} // namespace detail

template <typename Value, typename RTTI, typename Allocator>
struct map_type_map {
    map_type_map(Allocator& allocator) : values_(allocator) {}

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
    //    std::hash< typename RTTI::type_index>, std::equal_to< typename RTTI::type_index >,
    //    allocator_type > values2_;
};

template <typename Value, typename RTTI, typename Allocator>
struct sorted_vector_type_map {
    sorted_vector_type_map(Allocator& allocator) : values_(allocator) {}

    template <typename Key, typename ValueT>
    std::pair<Value&, bool> insert(ValueT&& value) {
        return values_.insert(RTTI::template get_type_index<Key>(),
                              std::forward<ValueT>(value));
    }

    template <typename Key> bool erase() {
        return values_.erase(RTTI::template get_type_index<Key>());
    }

    template <typename Key> Value* get() {
        return values_.get(RTTI::template get_type_index<Key>());
    }

    size_t size() const { return values_.size(); }

    Value& front() { return values_.front(); }

    auto begin() { return values_.begin(); }
    auto end() { return values_.end(); }

  private:
    detail::sorted_vector_table<typename RTTI::type_index, Value, Allocator>
        values_;
};

template <typename Value, typename RTTI, typename Allocator>
using dynamic_type_map = sorted_vector_type_map<Value, RTTI, Allocator>;

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
        assert(node.owner == this);

        auto current = &nodes_;
        while (*current && *current != &node) {
            current = &(*current)->next;
        }
        assert(*current == &node);
        *current = node.next;

        node.value.reset();
        node.next = nullptr;
        --size_;
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
