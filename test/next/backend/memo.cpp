//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Every index structure of a request memo returns the candidates of a key
// value that a linear scan of the candidates does, in the order of
// registration. spec: index_structures

#include <dingo/core/backend/memo.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

namespace {

using dingo::core::candidate;
using dingo::core::index_structure;
using dingo::core::lookup_descriptor;
using dingo::core::request_memo;
using dingo::core::runtime_key;

struct entry {};

// A key type that only compares equal, so only the linear index serves it.
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

// Registers candidates of the key values `keys`, indexes them by
// `declaration`, and compares every key value 0 to `range` with a linear scan.
template <typename Value>
void check(const lookup_descriptor &declaration, const std::vector<int> &keys,
           int range) {
  std::vector<entry> entries(keys.size());
  std::vector<Value> values;
  values.reserve(keys.size());
  request_memo<entry, runtime_key<Value>> memo;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    values.push_back(key_of<Value>(keys[index]));
    memo.add(candidate<entry>{&entries[index], index, &values.back()});
  }
  memo.index(declaration);
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

lookup_descriptor declared(index_structure index,
                           std::size_t bound = dingo::core::unbounded) {
  lookup_descriptor declaration;
  declaration.index = index;
  declaration.bound = bound;
  return declaration;
}

const std::vector<int> keys = {3, 1, 3, 0, 2, 3, 1, 0};

TEST(next_memo, every_index_returns_what_a_linear_scan_does) {
  for (index_structure index :
       {index_structure::ordered, index_structure::unordered,
        index_structure::sequence}) {
    check<int>(declared(index), keys, 5);
    check<channel>(declared(index), keys, 4);
  }
  check<int>(declared(index_structure::dense_array, 4), keys, 6);
  check<channel>(declared(index_structure::dense_array, 4), keys, 4);
  check<std::size_t>(declared(index_structure::dense_array, 4), keys, 6);
}

TEST(next_memo, a_key_type_without_an_order_or_a_hash_is_scanned) {
  for (index_structure index :
       {index_structure::ordered, index_structure::unordered,
        index_structure::dense_array}) {
    check<opaque_key>(declared(index, 4), keys, 5);
  }
}

TEST(next_memo, a_negative_key_is_outside_a_dense_array) {
  check<int>(declared(index_structure::dense_array, 4), keys, 4);
  std::vector<entry> entries(1);
  const int stored = 1;
  request_memo<entry, runtime_key<int>> memo;
  memo.add(candidate<entry>{&entries[0], 0, &stored});
  memo.index(declared(index_structure::dense_array, 4));
  const int negative = -1;
  bool visited = false;
  memo.visit(&negative, [&](const candidate<entry> &) {
    visited = true;
    return true;
  });
  EXPECT_FALSE(visited);
}

} // namespace
