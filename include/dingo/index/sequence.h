//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/index/index.h>

#include <array>
#include <utility>

namespace dingo {
namespace index_type {
template <template <typename...> typename Container> struct sequence {};
template <template <typename, size_t> typename Container, size_t N>
struct fixed_sequence {};
template <size_t N> using array = fixed_sequence<std::array, N>;
} // namespace index_type

namespace detail {
template <typename Key, typename Value> struct sequence_index_entry {
  Key first{};
  Value second{};
};

template <typename Entry> class sequence_index_iterator {
public:
  explicit sequence_index_iterator(Entry *value = nullptr) : value_(value) {}

  bool operator==(const sequence_index_iterator &other) const {
    return value_ == other.value_;
  }

  bool operator!=(const sequence_index_iterator &other) const {
    return !(*this == other);
  }

  Entry &operator*() const { return *value_; }

private:
  Entry *value_;
};

template <template <typename...> typename Container, typename Key,
          typename Value, typename Allocator>
struct index_storage<index_type::sequence<Container>, Key, Value, Allocator> {
  static_assert(std::is_integral_v<Key> && std::is_unsigned_v<Key>);

  using entry = sequence_index_entry<Key, Value>;
  using allocator_type =
      typename std::allocator_traits<Allocator>::template rebind_alloc<entry>;
  using sequence_type = Container<entry, allocator_type>;
  using iterator = sequence_index_iterator<entry>;

  explicit index_storage(Allocator &allocator) : values_(allocator) {}

  std::pair<iterator, bool> emplace(Key key, Value value) {
    const auto slot = static_cast<std::size_t>(key);
    if (slot >= values_.size())
      values_.resize(slot + 1);
    return emplace_at(slot, key, std::move(value));
  }

  iterator find(const Key &key) {
    const auto slot = static_cast<std::size_t>(key);
    if (slot < values_.size() && !values_[slot].second.is_empty())
      return iterator(&values_[slot]);
    return end();
  }

  iterator end() { return iterator(); }

private:
  std::pair<iterator, bool> emplace_at(std::size_t slot, Key key, Value value) {
    if (values_[slot].second.is_empty()) {
      values_[slot].first = key;
      values_[slot].second = std::move(value);
      return {iterator(&values_[slot]), true};
    }
    return {iterator(&values_[slot]), false};
  }

  sequence_type values_;
};

template <template <typename, size_t> typename Container, typename Key,
          typename Value, typename Allocator, size_t N>
struct index_storage<index_type::fixed_sequence<Container, N>, Key, Value,
                     Allocator> {
  static_assert(std::is_integral_v<Key> && std::is_unsigned_v<Key>);

  using entry = sequence_index_entry<Key, Value>;
  using sequence_type = Container<entry, N>;
  using iterator = sequence_index_iterator<entry>;

  explicit index_storage(Allocator &) {}

  std::pair<iterator, bool> emplace(Key key, Value value) {
    const auto slot = static_cast<std::size_t>(key);
    if (slot >= values_.size())
      throw detail::make_type_index_out_of_range_exception(key, values_.size());
    if (values_[slot].second.is_empty()) {
      values_[slot].first = key;
      values_[slot].second = std::move(value);
      return {iterator(&values_[slot]), true};
    }
    return {iterator(&values_[slot]), false};
  }

  iterator find(const Key &key) {
    const auto slot = static_cast<std::size_t>(key);
    if (slot < values_.size() && !values_[slot].second.is_empty())
      return iterator(&values_[slot]);
    return end();
  }

  iterator end() { return iterator(); }

private:
  sequence_type values_{};
};
} // namespace detail

} // namespace dingo
