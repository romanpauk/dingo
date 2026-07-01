//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/view/storage.h>

#include <cstddef>
#include <memory>
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

template <typename Key, typename Rows, typename Allocator, std::size_t Size>
class array_lookup_storage {
  struct slot {
    explicit slot(Allocator &allocator)
        : second(make_lookup_storage_allocator<typename Rows::allocator_type>(
              allocator)) {}

    bool engaged = false;
    Rows second;
  };

  using slot_allocator = lookup_storage_allocator_t<slot, Allocator>;
  using storage_type = std::vector<slot, slot_allocator>;

public:
  using iterator = slot *;
  using const_iterator = const slot *;

  explicit array_lookup_storage(Allocator &allocator)
      : values_(make_lookup_storage_allocator<slot_allocator>(allocator)) {
    values_.reserve(Size);
    for (std::size_t index = 0; index < Size; ++index) {
      values_.emplace_back(allocator);
    }
  }

  iterator find(const Key &key) {
    const auto index = to_index(key);
    if (!index || !values_[*index].engaged) {
      return end();
    }
    return std::addressof(values_[*index]);
  }

  const_iterator find(const Key &key) const {
    const auto index = to_index(key);
    if (!index || !values_[*index].engaged) {
      return end();
    }
    return std::addressof(values_[*index]);
  }

  iterator end() { return nullptr; }
  const_iterator end() const { return nullptr; }

  Rows &operator[](const Key &key) {
    const auto index = to_index(key);
    if (!index) {
      throw std::out_of_range("dingo array lookup key out of range");
    }
    values_[*index].engaged = true;
    return values_[*index].second;
  }

  iterator erase(iterator it) {
    it->second.clear();
    it->engaged = false;
    return end();
  }

private:
  static std::optional<std::size_t> to_index(const Key &key) {
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
    if (index >= Size) {
      return std::nullopt;
    }
    return index;
  }

  storage_type values_;
};

} // namespace detail

template <std::size_t Size> struct array {
  template <typename Key, typename Rows, typename Allocator>
  using storage = detail::array_lookup_storage<Key, Rows, Allocator, Size>;
};

} // namespace dingo
