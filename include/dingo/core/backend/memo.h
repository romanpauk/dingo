//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The memo of a request type in a runtime backend: the candidates of its
// domain, the bindings it finds before their keys are compared, in the order
// of registration, each with the term that serves the request. A memo is
// valid until the next registration. A memo of a request by a key value also
// indexes its candidates by their key values, by the index structure of its
// domain when the key type has what it needs, and otherwise by the first of
// the unordered, the ordered, and a linear index it has; an index returns the
// candidates the linear index does, in the same order. spec: index_structures

#include <dingo/core/engine/derive.h>
#include <dingo/core/engine/execute.h>
#include <dingo/core/engine/lookup.h>
#include <dingo/core/projection/key.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dingo::core {

namespace detail {
inline std::size_t next_request_id() {
  static std::atomic<std::size_t> next{0};
  return next.fetch_add(1, std::memory_order_relaxed);
}

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

struct no_index {};
} // namespace detail

// The process-wide identity of a request type by a key, which indexes the
// memos of a backend.
template <typename Request, typename Key> std::size_t request_id() {
  static const std::size_t id = detail::next_request_id();
  return id;
}

inline constexpr std::size_t no_generation = static_cast<std::size_t>(-1);

// A binding a memo holds: the entry, the term that serves the request, or
// no_term, and the value of its key. `kind` is the completion of the term,
// and `resolved` the address the term borrows once the binding is settled, so
// a resolution after it makes no call.
template <typename Entry> struct candidate {
  Entry *binding;
  std::size_t term;
  const void *key_value;
  completion kind = completion::construct;
  mutable void *resolved = nullptr;
};

class memo_base {
public:
  virtual ~memo_base() = default;

  std::size_t generation = no_generation;
};

// The memo of a request by a key without a value: every candidate matches.
template <typename Entry, typename Key, bool = key_has_value(Key::descriptor)>
class request_memo : public memo_base {
public:
  void clear() { candidates_.clear(); }

  void add(const candidate<Entry> &found) { candidates_.push_back(found); }

  void index(const lookup_descriptor & /*declaration*/) {}

  // Calls `visit` with the candidates of `key_value` in the order of
  // registration until it returns false.
  template <typename Visit>
  void visit(const void * /*key_value*/, Visit &&visit) const {
    for (const candidate<Entry> &found : candidates_) {
      if (!visit(found)) {
        return;
      }
    }
  }

private:
  std::vector<candidate<Entry>> candidates_;
};

// The memo of a request by a key value of type `Key::value_type`, which
// keeps a copy of the key value of every candidate next to it. The ordered
// index keeps the sorted key values apart from their candidates, so the
// search reads only keys.
template <typename Entry, typename Key>
class request_memo<Entry, Key, true> : public memo_base {
  using value_type = typename Key::value_type;
  static constexpr bool dense =
      std::is_integral_v<value_type> || std::is_enum_v<value_type>;
  static constexpr bool hashable = detail::is_hashable<value_type>::value;
  static constexpr bool ordered = detail::is_ordered<value_type>::value;

  struct keyed {
    value_type key;
    candidate<Entry> found;
  };

  // The candidates of one key value, a range of grouped_; `filled` counts
  // them while they are grouped.
  struct group {
    std::size_t first = unplaced;
    std::size_t count = 0;
    std::size_t filled = 0;
  };

  static constexpr std::size_t unplaced = static_cast<std::size_t>(-1);

  using unordered_index =
      std::conditional_t<hashable, std::unordered_map<value_type, group>,
                         detail::no_index>;

public:
  void clear() {
    candidates_.clear();
    keys_.clear();
    grouped_.clear();
    dense_.clear();
    if constexpr (hashable) {
      unordered_.clear();
    }
  }

  void add(const candidate<Entry> &found) {
    candidates_.push_back(
        {*static_cast<const value_type *>(found.key_value), found});
  }

  // Indexes the candidates by the index structure of their domain.
  void index(const lookup_descriptor &declaration) {
    kind_ = chosen(declaration);
    switch (kind_) {
    case index_structure::dense_array:
      if constexpr (dense) {
        dense_.assign(declaration.bound, group());
        group_by([&](const value_type &key) -> group & {
          return dense_[position(key)];
        });
      }
      break;
    case index_structure::unordered:
      if constexpr (hashable) {
        group_by(
            [&](const value_type &key) -> group & { return unordered_[key]; });
      }
      break;
    case index_structure::ordered:
      if constexpr (ordered) {
        std::stable_sort(candidates_.begin(), candidates_.end(),
                         [](const keyed &left, const keyed &right) {
                           return left.key < right.key;
                         });
        for (const keyed &each : candidates_) {
          keys_.push_back(each.key);
          grouped_.push_back(each.found);
        }
      }
      break;
    default:
      break;
    }
  }

  template <typename Visit>
  void visit(const void *key_value, Visit &&visit) const {
    const value_type &key = *static_cast<const value_type *>(key_value);
    switch (kind_) {
    case index_structure::dense_array:
      if constexpr (dense) {
        if (key_within(key, dense_.size())) {
          visit_group(dense_[position(key)], visit);
        }
        return;
      }
      break;
    case index_structure::unordered:
      if constexpr (hashable) {
        const auto found = unordered_.find(key);
        if (found != unordered_.end()) {
          visit_group(found->second, visit);
        }
        return;
      }
      break;
    case index_structure::ordered:
      if constexpr (ordered) {
        const auto first = std::lower_bound(keys_.begin(), keys_.end(), key);
        for (std::size_t index = std::size_t(first - keys_.begin());
             index != keys_.size() && !(key < keys_[index]); ++index) {
          if (!visit(grouped_[index])) {
            return;
          }
        }
        return;
      }
      break;
    default:
      break;
    }
    for (const keyed &each : candidates_) {
      if (each.key == key && !visit(each.found)) {
        return;
      }
    }
  }

private:
  static std::size_t position(const value_type &value) {
    if constexpr (std::is_enum_v<value_type>) {
      return static_cast<std::size_t>(
          static_cast<std::underlying_type_t<value_type>>(value));
    } else {
      return static_cast<std::size_t>(value);
    }
  }

  // The index structure of the declaration, if the key type has what it
  // needs, and otherwise the first the key type has; the sequence is the
  // linear index.
  static index_structure chosen(const lookup_descriptor &declaration) {
    if (declaration.index == index_structure::dense_array && dense &&
        declaration.bound != unbounded) {
      return index_structure::dense_array;
    }
    if (declaration.index == index_structure::ordered && ordered) {
      return index_structure::ordered;
    }
    if (hashable) {
      return index_structure::unordered;
    }
    return ordered ? index_structure::ordered : index_structure::sequence;
  }

  // Groups the candidates of each key value in grouped_, in the order of
  // registration, the groups in the order of their first candidates, and
  // records the range of each in the group `at` returns.
  template <typename At> void group_by(At &&at) {
    for (const keyed &each : candidates_) {
      ++at(each.key).count;
    }
    grouped_.resize(candidates_.size());
    std::size_t next = 0;
    for (const keyed &each : candidates_) {
      group &range = at(each.key);
      if (range.first == unplaced) {
        range.first = next;
        next += range.count;
      }
      grouped_[range.first + range.filled++] = each.found;
    }
  }

  template <typename Visit>
  void visit_group(const group &range, Visit &visit) const {
    for (std::size_t index = range.first; index != range.first + range.count;
         ++index) {
      if (!visit(grouped_[index])) {
        return;
      }
    }
  }

  index_structure kind_ = index_structure::sequence;
  std::vector<keyed> candidates_;
  std::vector<value_type> keys_;
  std::vector<candidate<Entry>> grouped_;
  std::vector<group> dense_;
  unordered_index unordered_;
};

} // namespace dingo::core
