//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <cassert>
#include <map>
#include <memory>
#include <tuple>
// #include <unordered_map>

namespace dingo {
template <typename Value, typename RTTI, typename Allocator>
struct dynamic_type_map {
  dynamic_type_map(Allocator &allocator) : values_(allocator) {}

  template <typename Key, typename... Args>
  std::pair<Value &, bool> insert(Args &&...args) {
    auto pb = values_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(RTTI::template get_type_index<Key>()),
        std::forward_as_tuple(std::forward<Args>(args)...));
    return {pb.first->second, pb.second};
  }

  template <typename Key> bool erase() {
    return values_.erase(RTTI::template get_type_index<Key>());
  }

  template <typename Key> Value *get() {
    auto it = values_.find(RTTI::template get_type_index<Key>());
    return it != values_.end() ? &it->second : nullptr;
  }

  size_t size() const { return values_.size(); }

  Value &front() {
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
  //    std::hash< typename RTTI::type_index>, std::equal_to< typename
  //    RTTI::type_index >, allocator_type > values2_;
};

} // namespace dingo
