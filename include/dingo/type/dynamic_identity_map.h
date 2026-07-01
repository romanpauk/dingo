//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <utility>

namespace dingo::detail {

template <typename Key, typename Value, typename Allocator,
          typename Compare = std::less<Key>>
class dynamic_identity_map {
  using storage_value = std::pair<const Key, Value>;
  using storage_allocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<storage_value>;
  using storage_type = std::map<Key, Value, Compare, storage_allocator>;

public:
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  explicit dynamic_identity_map(Allocator &allocator)
      : values_(Compare{}, storage_allocator(allocator)) {}

  template <typename KeyArg, typename... Args>
  std::pair<Value &, bool> insert(KeyArg &&key, Args &&...args) {
    auto pb =
        values_.emplace(std::piecewise_construct,
                        std::forward_as_tuple(std::forward<KeyArg>(key)),
                        std::forward_as_tuple(std::forward<Args>(args)...));
    return {pb.first->second, pb.second};
  }

  Value *get(const Key &key) {
    auto it = values_.find(key);
    return it != values_.end() ? &it->second : nullptr;
  }

  const Value *get(const Key &key) const {
    auto it = values_.find(key);
    return it != values_.end() ? &it->second : nullptr;
  }

  bool erase(const Key &key) { return values_.erase(key) != 0; }

  iterator erase(iterator it) { return values_.erase(it); }

  bool empty() const { return values_.empty(); }

  size_t size() const { return values_.size(); }

  Value &front() { return values_.begin()->second; }

  iterator begin() { return values_.begin(); }
  iterator end() { return values_.end(); }
  const_iterator begin() const { return values_.begin(); }
  const_iterator end() const { return values_.end(); }

private:
  storage_type values_;
};

} // namespace dingo::detail
