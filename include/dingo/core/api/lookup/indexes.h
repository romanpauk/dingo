//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The indexes of the lookup backends of the library: a linear scan, the
// sorted key values, a hash table, and a dense array of a bound, each an index
// as core/backend/index.h describes it, which a backend of the user could
// name too. One the key type cannot serve falls back to one it can: an
// ordered index to a hash table, a hash table to the sorted key values, a
// dense array to a hash table, and each to a linear scan.
// spec: index_structures

#include <dingo/core/backend/index.h>
#include <dingo/core/engine/lookup.h>
#include <dingo/core/projection/key.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace dingo::core {

namespace detail {
template <typename Type, typename = void>
struct is_hashable : std::false_type {};

template <typename Type>
struct is_hashable<
    Type,
    std::void_t<decltype(std::hash<Type>()(std::declval<const Type &>()) ==
                         std::size_t())>> : std::true_type {};

template <typename Type, typename = void>
struct is_ordered : std::false_type {};

template <typename Type>
struct is_ordered<Type,
                  std::void_t<decltype(bool(std::declval<const Type &>() <
                                            std::declval<const Type &>()))>>
    : std::true_type {};

// The candidates of one key value, a range of the grouped candidates.
struct index_group {
  static constexpr std::size_t unplaced = static_cast<std::size_t>(-1);
  std::size_t first = unplaced;
  std::size_t count = 0;
  std::size_t filled = 0;
};

// Groups the candidates of each key value in `grouped`, in the order of
// registration, the groups in the order of their first candidates, and
// records the range of each in the group `at` returns.
template <typename Value, typename Candidate, typename At>
void group_items(const index_item<Value, Candidate> *items, std::size_t count,
                 std::vector<Candidate> &grouped, At &&at) {
  for (std::size_t index = 0; index < count; ++index) {
    ++at(items[index].key).count;
  }
  grouped.resize(count);
  std::size_t next = 0;
  for (std::size_t index = 0; index < count; ++index) {
    index_group &range = at(items[index].key);
    if (range.first == index_group::unplaced) {
      range.first = next;
      next += range.count;
    }
    grouped[range.first + range.filled++] = items[index].candidate;
  }
}

template <typename Candidate, typename Visit>
void visit_group(const std::vector<Candidate> &grouped,
                 const index_group &range, Visit &visit) {
  for (std::size_t index = range.first; index != range.first + range.count;
       ++index) {
    if (!visit(grouped[index])) {
      return;
    }
  }
}
} // namespace detail

// A linear scan of the candidates, which needs only an equality of keys.
template <typename Value, typename Candidate> class linear_index {
public:
  void build(const index_item<Value, Candidate> *items, std::size_t count) {
    items_ = items;
    count_ = count;
  }

  template <typename Visit> void visit(const Value &key, Visit &&visit) const {
    for (std::size_t index = 0; index < count_; ++index) {
      if (items_[index].key == key && !visit(items_[index].candidate)) {
        return;
      }
    }
  }

private:
  const index_item<Value, Candidate> *items_ = nullptr;
  std::size_t count_ = 0;
};

// The sorted key values apart from their candidates, so a search reads only
// keys.
template <typename Value, typename Candidate> class sorted_index {
public:
  void build(const index_item<Value, Candidate> *items, std::size_t count) {
    std::vector<index_item<Value, Candidate>> sorted(items, items + count);
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const auto &left, const auto &right) {
                       return left.key < right.key;
                     });
    keys_.clear();
    grouped_.clear();
    for (const auto &each : sorted) {
      keys_.push_back(each.key);
      grouped_.push_back(each.candidate);
    }
  }

  template <typename Visit> void visit(const Value &key, Visit &&visit) const {
    const auto first = std::lower_bound(keys_.begin(), keys_.end(), key);
    for (std::size_t index = std::size_t(first - keys_.begin());
         index != keys_.size() && !(key < keys_[index]); ++index) {
      if (!visit(grouped_[index])) {
        return;
      }
    }
  }

private:
  std::vector<Value> keys_;
  std::vector<Candidate> grouped_;
};

// A hash table of the groups of the key values.
template <typename Value, typename Candidate> class hashed_index {
public:
  void build(const index_item<Value, Candidate> *items, std::size_t count) {
    groups_.clear();
    detail::group_items(items, count, grouped_,
                        [this](const Value &key) -> detail::index_group & {
                          return groups_[key];
                        });
  }

  template <typename Visit> void visit(const Value &key, Visit &&visit) const {
    const auto found = groups_.find(key);
    if (found != groups_.end()) {
      detail::visit_group(grouped_, found->second, visit);
    }
  }

private:
  std::unordered_map<Value, detail::index_group> groups_;
  std::vector<Candidate> grouped_;
};

// A dense array of the groups of the key values 0 up to `Bound`, excluded.
template <std::size_t Bound, typename Value, typename Candidate>
class dense_index {
public:
  void build(const index_item<Value, Candidate> *items, std::size_t count) {
    groups_.assign(Bound, detail::index_group());
    detail::group_items(items, count, grouped_,
                        [this](const Value &key) -> detail::index_group & {
                          return groups_[position(key)];
                        });
  }

  template <typename Visit> void visit(const Value &key, Visit &&visit) const {
    if (key_within(key, Bound)) {
      detail::visit_group(grouped_, groups_[position(key)], visit);
    }
  }

private:
  static std::size_t position(const Value &value) {
    if constexpr (std::is_enum_v<Value>) {
      return static_cast<std::size_t>(
          static_cast<std::underlying_type_t<Value>>(value));
    } else {
      return static_cast<std::size_t>(value);
    }
  }

  std::vector<detail::index_group> groups_;
  std::vector<Candidate> grouped_;
};

// The indexes of the backends of the library with their fallbacks.
template <typename Value, typename Candidate>
using ordered_index_t =
    std::conditional_t<detail::is_ordered<Value>::value,
                       sorted_index<Value, Candidate>,
                       std::conditional_t<detail::is_hashable<Value>::value,
                                          hashed_index<Value, Candidate>,
                                          linear_index<Value, Candidate>>>;

template <typename Value, typename Candidate>
using hashed_index_t =
    std::conditional_t<detail::is_hashable<Value>::value,
                       hashed_index<Value, Candidate>,
                       std::conditional_t<detail::is_ordered<Value>::value,
                                          sorted_index<Value, Candidate>,
                                          linear_index<Value, Candidate>>>;

template <std::size_t Bound, typename Value, typename Candidate>
using dense_index_t = std::conditional_t<
    (std::is_integral_v<Value> || std::is_enum_v<Value>) && Bound != unbounded,
    dense_index<Bound, Value, Candidate>, hashed_index_t<Value, Candidate>>;

} // namespace dingo::core
