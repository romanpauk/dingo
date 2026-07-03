//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/lookup.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo::detail {

template <typename Value> struct lookup_singular_result {
  Value *value = nullptr;
  bool ambiguous = false;
};

template <typename Value>
auto lookup_value(Value &value, int) -> decltype((value.second)) {
  return value.second;
}

template <typename Value> Value &lookup_value(Value &value, ...) {
  return value;
}

template <typename Iterator> decltype(auto) lookup_iterator_value(Iterator it) {
  return lookup_value(*it, 0);
}

template <typename LookupEntry, typename Value, typename Backend, typename Key>
lookup_singular_result<Value> lookup_find_singular(Backend &backend,
                                                   const Key &key) {
  if constexpr (std::is_same_v<
                    typename lookup_entry_cardinality<LookupEntry>::type,
                    ::dingo::one>) {
    auto it = backend.find(key);
    if (it == backend.end()) {
      return {};
    }
    return {std::addressof(lookup_iterator_value(it)), false};
  } else {
    auto [first, last] = backend.equal_range(key);
    if (first == last) {
      return {};
    }
    auto next = first;
    ++next;
    const bool ambiguous = next != last;
    return {ambiguous ? nullptr : std::addressof(lookup_iterator_value(first)),
            ambiguous};
  }
}

template <typename LookupEntry, typename Backend, typename Key, typename Fn>
std::size_t lookup_for_each(Backend &backend, const Key &key, Fn &&fn) {
  if constexpr (std::is_same_v<
                    typename lookup_entry_cardinality<LookupEntry>::type,
                    ::dingo::one>) {
    auto it = backend.find(key);
    if (it == backend.end()) {
      return 0;
    }
    fn(lookup_iterator_value(it));
    return 1;
  } else {
    std::size_t result = 0;
    auto [first, last] = backend.equal_range(key);
    for (auto it = first; it != last; ++it) {
      fn(lookup_iterator_value(it));
      ++result;
    }
    return result;
  }
}

} // namespace dingo::detail
