//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Every index a lookup backend names returns the candidates of a key value
// that a linear scan of the candidates does, in the order of registration.
// spec: index_structures

#include <dingo/core/backend/memo.h>
#include <dingo/lookup/lookup.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <unordered_map>
#include <vector>

namespace {

using dingo::core::candidate;
using dingo::core::request_memo;
using dingo::core::runtime_key;

struct entry {};

// A key type that only compares equal, so only a linear scan serves it.
struct opaque_key {
  int value;
  bool operator==(const opaque_key &other) const {
    return value == other.value;
  }
};

enum class channel : int { first, second, third, fourth };

template <typename Value> Value key_of(int value) {
  if constexpr (std::is_same_v<Value, opaque_key>) {
    return opaque_key{value};
  } else {
    return static_cast<Value>(value);
  }
}

// A backend whose index is a linear scan.
struct linear {
  template <typename Value, typename Candidate>
  using index = dingo::core::linear_index<Value, Candidate>;
};

// Backends of the user that name a map as their storage: one that keeps the
// key values in order, and one that keeps equal keys in no order.
struct ordered_storage {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage = std::map<Key, Value>;
};

struct hashed_storage {
  template <typename Key, typename Value, typename Cardinality,
            typename Allocator>
  using storage = std::unordered_multimap<Key, Value>;
};

template <typename Backend>
using indexing_t =
    typename dingo::detail::backend_indexing<Backend, dingo::many>::type;

// Registers candidates of the key values `keys`, indexes them by the index of
// `Backend`, and compares every key value 0 to `range` with a linear scan.
template <typename Value, typename Backend>
void check(const std::vector<int> &keys, int range) {
  std::vector<entry> entries(keys.size());
  std::vector<Value> values;
  values.reserve(keys.size());
  request_memo<entry, runtime_key<Value>, indexing_t<Backend>> memo;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    values.push_back(key_of<Value>(keys[index]));
    memo.add(candidate<entry>{&entries[index], index, &values.back()});
  }
  memo.index();
  for (int key = 0; key < range; ++key) {
    const Value value = key_of<Value>(key);
    std::vector<std::size_t> expected;
    for (std::size_t index = 0; index < keys.size(); ++index) {
      if (keys[index] == key) {
        expected.push_back(index);
      }
    }
    std::vector<std::size_t> found;
    memo.visit(&value, [&](const candidate<entry> &each) {
      found.push_back(each.term);
      return true;
    });
    EXPECT_EQ(found, expected) << "key " << key;
  }
}

const std::vector<int> keys = {3, 1, 3, 0, 2, 3, 1, 0};

template <typename Backend> void check_backend(int range) {
  check<int, Backend>(keys, range);
  check<channel, Backend>(keys, 4);
  check<std::size_t, Backend>(keys, range);
}

TEST(next_memo, every_index_returns_what_a_linear_scan_does) {
  check_backend<dingo::ordered>(5);
  check_backend<dingo::unordered>(5);
  check_backend<dingo::array<4>>(6);
  check_backend<linear>(5);
}

TEST(next_memo, a_map_of_the_user_indexes_in_the_order_of_registration) {
  check<int, hashed_storage>(keys, 5);
  check<std::size_t, hashed_storage>(keys, 5);
  check<int, ordered_storage>(keys, 5);
}

TEST(next_memo, a_key_type_without_an_order_or_a_hash_is_scanned) {
  check<opaque_key, dingo::ordered>(keys, 5);
  check<opaque_key, dingo::unordered>(keys, 5);
  check<opaque_key, dingo::array<4>>(keys, 5);
}

TEST(next_memo, a_negative_key_is_outside_a_dense_array) {
  check<int, dingo::array<4>>(keys, 4);
  std::vector<entry> entries(1);
  const int stored = 1;
  request_memo<entry, runtime_key<int>, indexing_t<dingo::array<4>>> memo;
  memo.add(candidate<entry>{&entries[0], 0, &stored});
  memo.index();
  const int negative = -1;
  bool visited = false;
  memo.visit(&negative, [&](const candidate<entry> &) {
    visited = true;
    return true;
  });
  EXPECT_FALSE(visited);
}

} // namespace
