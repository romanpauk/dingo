//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/none.h>
#include <dingo/base/type_list.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/storage.h>

#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::detail {

template <typename Value, typename Cardinality, typename Allocator>
class static_key_lookup_storage;

// The one value of a static key, found through a pointer to it.
template <typename Value, typename Allocator>
class static_key_lookup_storage<Value, ::dingo::one, Allocator> {
public:
  using iterator = Value *;

  explicit static_key_lookup_storage(Allocator &) {}

  iterator find(::dingo::none_t) { return value_ ? &*value_ : end(); }

  iterator end() { return nullptr; }

  template <typename Inserted>
  std::pair<iterator, bool> try_emplace(::dingo::none_t, Inserted &&value) {
    if (value_) {
      return {&*value_, false};
    }
    value_.emplace(std::forward<Inserted>(value));
    return {&*value_, true};
  }

  void erase(iterator handle) {
    if (value_ && handle == &*value_) {
      value_.reset();
    }
  }

private:
  std::optional<Value> value_;
};

template <typename Value, typename Allocator>
class static_key_lookup_storage<Value, ::dingo::many, Allocator> {
  using value_allocator = lookup_storage_allocator_t<Value, Allocator>;
  using storage_type = std::vector<Value, value_allocator>;

public:
  using iterator = typename storage_type::iterator;

  explicit static_key_lookup_storage(Allocator &allocator)
      : values_(make_lookup_storage_allocator<value_allocator>(allocator)) {}

  iterator find(::dingo::none_t) {
    return values_.empty() ? values_.end() : values_.begin();
  }

  iterator end() { return values_.end(); }

  std::pair<iterator, iterator> equal_range(::dingo::none_t) {
    return {values_.begin(), values_.end()};
  }

  template <typename Inserted>
  iterator emplace(::dingo::none_t, Inserted &&value) {
    values_.emplace_back(std::forward<Inserted>(value));
    auto it = values_.end();
    --it;
    return it;
  }

  void erase(iterator handle) { values_.erase(handle); }

private:
  storage_type values_;
};

template <typename BackendKey, typename Backend, typename Value,
          typename Cardinality, typename Allocator>
struct lookup_backend_storage_type {
  using type = typename Backend::template storage<BackendKey, Value,
                                                  Cardinality, Allocator>;
};

template <typename Backend, typename Value, typename Cardinality,
          typename Allocator>
struct lookup_backend_storage_type<::dingo::none_t, Backend, Value, Cardinality,
                                   Allocator> {
  using type = static_key_lookup_storage<Value, Cardinality, Allocator>;
};

template <typename Entry, typename Value, typename Allocator>
struct lookup_backend_storage;

template <typename Interface, typename KeyDefinition, typename Cardinality,
          typename Backend, typename Value, typename Allocator>
struct lookup_backend_storage<
    lookup_entry<Interface, KeyDefinition, Cardinality, Backend>, Value,
    Allocator> {
  using type = typename lookup_backend_storage_type<
      typename KeyDefinition::backend_key_type, Backend, Value, Cardinality,
      Allocator>::type;
};

template <typename Entry, typename Value, typename Allocator>
class lookup_backend
    : public lookup_backend_storage<Entry, Value, Allocator>::type {
  using base_type =
      typename lookup_backend_storage<Entry, Value, Allocator>::type;

public:
  explicit lookup_backend(Allocator &allocator)
      : base_type(make_lookup_storage<base_type>(allocator)) {}
};

template <typename Entry, typename Value, typename Allocator>
struct lookup_index_member {
  explicit lookup_index_member(Allocator &allocator) : backend(allocator) {}

  lookup_backend<Entry, Value, Allocator> backend;
};

template <typename EntryList, typename Value, typename Allocator>
class lookup_index;

template <typename... Entries, typename Value, typename Allocator>
class lookup_index<::dingo::type_list<Entries...>, Value, Allocator>
    : lookup_index_member<Entries, Value, Allocator>... {
public:
  explicit lookup_index(Allocator &allocator)
      : lookup_index_member<Entries, Value, Allocator>(allocator)... {}

  template <typename Entry> lookup_backend<Entry, Value, Allocator> &get() {
    return lookup_index_member<Entry, Value, Allocator>::backend;
  }

  template <typename Entry>
  const lookup_backend<Entry, Value, Allocator> &get() const {
    return lookup_index_member<Entry, Value, Allocator>::backend;
  }
};

} // namespace dingo::detail
