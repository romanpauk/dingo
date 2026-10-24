//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The memo of a request type in a runtime backend: the candidates of its
// domain, the bindings it finds before their keys are compared, in the order
// of registration, each with the reachable type that serves the request. A memo
// is valid until the next registration. A memo of a request by a key value also
// indexes its candidates by their key values, by the index the lookup
// declaration of its domain names, see index.h. spec: index_structures

#include <dingo/conversion/convert.h>
#include <dingo/conversion/route.h>
#include <dingo/lookup/declarations.h>
#include <dingo/lookup/index.h>
#include <dingo/lookup/key.h>

#include <atomic>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {

namespace detail {
inline std::size_t next_request_id() {
  static std::atomic<std::size_t> next{0};
  return next.fetch_add(1, std::memory_order_relaxed);
}

} // namespace detail

// The process-wide identity of a request type by a key, which indexes the
// memos of a backend. It is a function-local static, so a request made while
// the program initializes has its identity.
template <typename Request, typename Key>
DINGO_ALWAYS_INLINE std::size_t request_id() {
  static const std::size_t id = detail::next_request_id();
  return id;
}

inline constexpr std::size_t no_generation = static_cast<std::size_t>(-1);

// A binding a memo holds: the entry, the reachable type that serves the
// request, or no_reachable_type, and the value of its key. `plan` is how the
// request completes the reachable type, and `resolved` the address the
// reachable type borrows once the binding is settled, so a resolution after it
// makes no call.
template <typename Entry> struct candidate {
  Entry *binding;
  std::size_t reachable_type;
  const void *key_value;
  request_plan plan = {};
  mutable void *resolved = nullptr;
};

class memo_base {
public:
  virtual ~memo_base() = default;

  std::size_t generation = no_generation;
};

// The memo of a request by a key without a value: every candidate matches.
// `Indexing` names the index of the candidates of a request by a key value.
template <typename Entry, typename Key, typename Indexing,
          bool = key_has_value(Key::descriptor)>
class request_memo : public memo_base {
public:
  void clear() { candidates_.clear(); }

  void add(const candidate<Entry> &found) { candidates_.push_back(found); }

  void index() {}

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
// keeps a copy of the key value of every candidate next to it, and indexes
// them by the index of `Indexing`.
template <typename Entry, typename Key, typename Indexing>
class request_memo<Entry, Key, Indexing, true> : public memo_base {
  using value_type = typename Key::value_type;
  using item = index_item<value_type, candidate<Entry>>;
  using index_type =
      typename Indexing::template index<value_type, candidate<Entry>>;

public:
  void clear() {
    items_.clear();
    index_ = index_type();
  }

  void add(const candidate<Entry> &found) {
    items_.push_back(
        {*static_cast<const value_type *>(found.key_value), found});
  }

  void index() { index_.build(items_.data(), items_.size()); }

  // Calls `visit` with the candidates of `key_value` in the order of
  // registration until it returns false.
  template <typename Visit>
  void visit(const void *key_value, Visit &&visit) const {
    index_.visit(*static_cast<const value_type *>(key_value), visit);
  }

private:
  std::vector<item> items_;
  index_type index_;
};

} // namespace dingo
