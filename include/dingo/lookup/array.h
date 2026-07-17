//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/storage.h>
#include <dingo/lookup/tags.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace dingo {
namespace detail {

template <typename Key, bool IsEnum = std::is_enum_v<Key>>
struct array_position_type {
  using type = Key;
};

template <typename Key> struct array_position_type<Key, true> {
  using type = std::underlying_type_t<Key>;
};

template <typename Key>
std::optional<std::size_t> array_lookup_index(Key key, std::size_t size) {
  static_assert(std::is_integral_v<Key> || std::is_enum_v<Key>,
                "dingo::array<N> associative backend requires an integral "
                "or enum key");
  using raw_position_type = typename array_position_type<Key>::type;
  const auto raw_position = static_cast<raw_position_type>(key);
  if constexpr (std::is_signed_v<raw_position_type>) {
    if (raw_position < 0) {
      return std::nullopt;
    }
  }
  const auto index = static_cast<std::size_t>(raw_position);
  if (index >= size) {
    return std::nullopt;
  }
  return index;
}

template <typename Key, typename Value, typename Cardinality,
          typename Allocator, std::size_t Size>
class array_lookup_storage;

template <typename Key, typename Value, typename Allocator, std::size_t Size>
class array_lookup_storage<Key, Value, ::dingo::one, Allocator, Size> {
  struct slot {
    std::optional<Value> second;
  };

public:
  class iterator {
  public:
    iterator() = default;

    Value &operator*() const { return *slot_->second; }

    bool operator==(const iterator &other) const {
      return slot_ == other.slot_;
    }

    bool operator!=(const iterator &other) const { return !(*this == other); }

  private:
    friend class array_lookup_storage;

    explicit iterator(slot *resolved_slot) : slot_(resolved_slot) {}

    slot *slot_ = nullptr;
  };

  class const_iterator {
  public:
    const_iterator() = default;

    const Value &operator*() const { return *slot_->second; }

    bool operator==(const const_iterator &other) const {
      return slot_ == other.slot_;
    }

    bool operator!=(const const_iterator &other) const {
      return !(*this == other);
    }

  private:
    friend class array_lookup_storage;

    explicit const_iterator(const slot *resolved_slot) : slot_(resolved_slot) {}

    const slot *slot_ = nullptr;
  };

  explicit array_lookup_storage(Allocator &) {}

  iterator find(const Key &key) {
    const auto index = to_index(key);
    return index && entries_[*index].second ? iterator(&entries_[*index])
                                            : end();
  }

  const_iterator find(const Key &key) const {
    const auto index = to_index(key);
    return index && entries_[*index].second ? const_iterator(&entries_[*index])
                                            : end();
  }

  iterator end() { return iterator(); }
  const_iterator end() const { return const_iterator(); }

  template <typename Inserted>
  std::pair<iterator, bool> try_emplace(const Key &key, Inserted &&value) {
    const auto index = checked_index(key);
    auto &entry = entries_[index].second;
    if (entry) {
      return {iterator(&entries_[index]), false};
    }
    entry.emplace(std::forward<Inserted>(value));
    return {iterator(&entries_[index]), true};
  }

  void erase(iterator handle) {
    if (handle.slot_) {
      handle.slot_->second.reset();
    }
  }

  template <typename Visitor> void for_each(Visitor &&visitor) const {
    for (std::size_t index = 0; index < Size; ++index) {
      if (entries_[index].second) {
        visitor(static_cast<Key>(index), *entries_[index].second);
      }
    }
  }

private:
  static std::optional<std::size_t> to_index(const Key &key) {
    return array_lookup_index(key, Size);
  }

  static std::size_t checked_index(const Key &key) {
    auto index = to_index(key);
    if (!index) {
      throw std::out_of_range("dingo array lookup key out of range");
    }
    return *index;
  }

  std::array<slot, Size> entries_{};
};

template <typename Key, typename Value, typename Allocator, std::size_t Size>
class array_lookup_storage<Key, Value, ::dingo::many, Allocator, Size> {
  using mapped_allocator = lookup_storage_allocator_t<Value, Allocator>;
  using bucket_type = std::vector<Value, mapped_allocator>;
  using bucket_allocator = lookup_storage_allocator_t<bucket_type, Allocator>;
  using storage_type = std::vector<bucket_type, bucket_allocator>;

public:
  class iterator {
  public:
    iterator() = default;

    Value &operator*() const { return *it_; }

    iterator &operator++() {
      ++it_;
      return *this;
    }

    bool operator==(const iterator &other) const {
      return owner_ == other.owner_ && index_ == other.index_ &&
             (is_end() || it_ == other.it_);
    }

    bool operator!=(const iterator &other) const { return !(*this == other); }

  private:
    friend class array_lookup_storage;

    iterator(array_lookup_storage *owner, std::size_t index,
             typename bucket_type::iterator it)
        : owner_(owner), index_(index), it_(it) {}

    bool is_end() const { return !owner_ || index_ == Size; }

    array_lookup_storage *owner_ = nullptr;
    std::size_t index_ = Size;
    typename bucket_type::iterator it_{};
  };

  class const_iterator {
  public:
    const_iterator() = default;

    const Value &operator*() const { return *it_; }

    const_iterator &operator++() {
      ++it_;
      return *this;
    }

    bool operator==(const const_iterator &other) const {
      return owner_ == other.owner_ && index_ == other.index_ &&
             (is_end() || it_ == other.it_);
    }

    bool operator!=(const const_iterator &other) const {
      return !(*this == other);
    }

  private:
    friend class array_lookup_storage;

    const_iterator(const array_lookup_storage *owner, std::size_t index,
                   typename bucket_type::const_iterator it)
        : owner_(owner), index_(index), it_(it) {}

    bool is_end() const { return !owner_ || index_ == Size; }

    const array_lookup_storage *owner_ = nullptr;
    std::size_t index_ = Size;
    typename bucket_type::const_iterator it_{};
  };

  explicit array_lookup_storage(Allocator &allocator)
      : values_(make_lookup_storage_allocator<bucket_allocator>(allocator)),
        mapped_allocator_(
            make_lookup_storage_allocator<mapped_allocator>(allocator)) {
    values_.reserve(Size);
    for (std::size_t index = 0; index < Size; ++index) {
      values_.emplace_back(mapped_allocator_);
    }
  }

  iterator find(const Key &key) {
    const auto range = equal_range(key);
    return range.first == range.second ? end() : range.first;
  }

  const_iterator find(const Key &key) const {
    const auto range = equal_range(key);
    return range.first == range.second ? end() : range.first;
  }

  iterator end() { return iterator(this, Size, {}); }
  const_iterator end() const { return const_iterator(this, Size, {}); }

  std::pair<iterator, iterator> equal_range(const Key &key) {
    const auto index = to_index(key);
    if (!index) {
      return {end(), end()};
    }
    auto &bucket = values_[*index];
    return {iterator(this, *index, bucket.begin()),
            iterator(this, *index, bucket.end())};
  }

  std::pair<const_iterator, const_iterator> equal_range(const Key &key) const {
    const auto index = to_index(key);
    if (!index) {
      return {end(), end()};
    }
    const auto &bucket = values_[*index];
    return {const_iterator(this, *index, bucket.begin()),
            const_iterator(this, *index, bucket.end())};
  }

  template <typename Inserted>
  iterator emplace(const Key &key, Inserted &&value) {
    const auto index = checked_index(key);
    auto &bucket = values_[index];
    bucket.emplace_back(std::forward<Inserted>(value));
    auto it = bucket.end();
    --it;
    return iterator(this, index, it);
  }

  void erase(iterator handle) { values_[handle.index_].erase(handle.it_); }

  template <typename Visitor> void for_each(Visitor &&visitor) const {
    for (std::size_t index = 0; index < Size; ++index) {
      for (const auto &value : values_[index]) {
        visitor(static_cast<Key>(index), value);
      }
    }
  }

private:
  static std::optional<std::size_t> to_index(const Key &key) {
    return array_lookup_index(key, Size);
  }

  static std::size_t checked_index(const Key &key) {
    auto index = to_index(key);
    if (!index) {
      throw std::out_of_range("dingo array lookup key out of range");
    }
    return *index;
  }

  storage_type values_;
  mapped_allocator mapped_allocator_;
};

} // namespace detail

template <std::size_t Size> struct array {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage =
      detail::array_lookup_storage<Key, Value, Cardinality, Allocator, Size>;
};

} // namespace dingo
