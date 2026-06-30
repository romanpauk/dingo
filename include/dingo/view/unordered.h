//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/view/storage.h>

#include <functional>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace dingo {
namespace detail {

template <typename Key, typename Rows, typename Allocator>
class unordered_lookup_storage
    : public std::unordered_map<
          Key, Rows, std::hash<Key>, std::equal_to<Key>,
          lookup_storage_allocator_t<std::pair<const Key, Rows>, Allocator>> {
  using base = std::unordered_map<
      Key, Rows, std::hash<Key>, std::equal_to<Key>,
      lookup_storage_allocator_t<std::pair<const Key, Rows>, Allocator>>;

public:
  explicit unordered_lookup_storage(Allocator &allocator)
      : base(0, std::hash<Key>{}, std::equal_to<Key>{},
             make_lookup_storage_allocator<typename base::allocator_type>(
                 allocator)),
        allocator_(std::addressof(allocator)) {}

  Rows &operator[](const Key &key) {
    auto it = this->find(key);
    if (it == this->end()) {
      it = this->emplace(std::piecewise_construct, std::forward_as_tuple(key),
                         std::forward_as_tuple(
                             make_lookup_storage_allocator<
                                 typename Rows::allocator_type>(*allocator_)))
               .first;
    }
    return it->second;
  }

private:
  Allocator *allocator_;
};

} // namespace detail

struct unordered {
  template <typename Key, typename Rows, typename Allocator>
  using storage = detail::unordered_lookup_storage<Key, Rows, Allocator>;
};

} // namespace dingo
