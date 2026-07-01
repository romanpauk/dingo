//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/none.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/view/view.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::detail {

enum class slot_domain { no_key, typed_key, runtime_key };
enum class slot_cardinality { one, many };

template <typename Cardinality>
constexpr slot_cardinality slot_cardinality_value() {
  if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
    return slot_cardinality::one;
  } else {
    return slot_cardinality::many;
  }
}

template <typename LookupEntry, bool = std::is_void_v<LookupEntry>>
struct lookup_entry_cardinality {
  using type = typename LookupEntry::cardinality;
};

template <typename LookupEntry>
struct lookup_entry_cardinality<LookupEntry, true> {
  using type = ::dingo::one;
};

template <typename KeyDomain> struct is_runtime_key_domain : std::false_type {};

template <typename Key>
struct is_runtime_key_domain<::dingo::runtime_key<Key>> : std::true_type {};

template <typename LookupEntry>
inline constexpr bool is_runtime_key_lookup_entry_v =
    is_runtime_key_domain<typename LookupEntry::key_domain>::value;

template <typename LookupEntry, typename Entry, typename Allocator>
class slot_storage;

template <typename Entry, typename Allocator>
using slot_storage_row_allocator_t = typename std::conditional_t<
    ::dingo::is_static_allocator_v<Allocator>, std::allocator<Entry *>,
    typename std::allocator_traits<Allocator>::template rebind_alloc<Entry *>>;

template <typename Entry, typename Allocator, slot_cardinality Cardinality>
class static_key_slot_storage;

template <typename Entry, typename Allocator>
class static_key_slot_storage<Entry, Allocator, slot_cardinality::one> {
public:
  struct insertion_handle {
    Entry *entry = nullptr;
  };

  struct insertion_result {
    bool inserted;
    insertion_handle handle;

    explicit operator bool() const { return inserted; }
  };

  explicit static_key_slot_storage(Allocator &) {}

  bool conflicts(const Entry &) const { return entry_ != nullptr; }

  bool conflicts(const Entry &entry, ::dingo::none_t) const {
    return conflicts(entry);
  }

  insertion_result insert(Entry &entry) {
    if (entry_) {
      return {false, {}};
    }
    entry_ = std::addressof(entry);
    return {true, {entry_}};
  }

  insertion_result insert(Entry &entry, ::dingo::none_t) {
    return insert(entry);
  }

  void erase(insertion_handle handle) {
    if (entry_ == handle.entry) {
      entry_ = nullptr;
    }
  }

  void erase(Entry &entry) { erase({std::addressof(entry)}); }

  void erase(Entry &entry, ::dingo::none_t) { erase(entry); }

  Entry *find_singular(bool &ambiguous) const {
    ambiguous = false;
    return entry_;
  }

  Entry *find_singular(::dingo::none_t, bool &ambiguous) const {
    return find_singular(ambiguous);
  }

  template <typename Fn> std::size_t for_each(Fn &&fn) const {
    if (!entry_) {
      return 0;
    }
    fn(*entry_);
    return 1;
  }

  template <typename Fn> std::size_t for_each(::dingo::none_t, Fn &&fn) const {
    return for_each(std::forward<Fn>(fn));
  }

private:
  Entry *entry_ = nullptr;
};

template <typename Entry, typename Allocator>
class static_key_slot_storage<Entry, Allocator, slot_cardinality::many> {
  using row_allocator = slot_storage_row_allocator_t<Entry, Allocator>;
  using entries_type = std::vector<Entry *, row_allocator>;

public:
  struct insertion_handle {
    Entry *entry = nullptr;
  };

  struct insertion_result {
    bool inserted;
    insertion_handle handle;

    explicit operator bool() const { return inserted; }
  };

  explicit static_key_slot_storage(Allocator &allocator)
      : entries_(make_lookup_storage_allocator<row_allocator>(allocator)) {}

  bool conflicts(const Entry &) const { return false; }

  bool conflicts(const Entry &entry, ::dingo::none_t) const {
    return conflicts(entry);
  }

  insertion_result insert(Entry &entry) {
    entries_.push_back(std::addressof(entry));
    return {true, {std::addressof(entry)}};
  }

  insertion_result insert(Entry &entry, ::dingo::none_t) {
    return insert(entry);
  }

  void erase(insertion_handle handle) {
    auto it = std::find(entries_.begin(), entries_.end(), handle.entry);
    if (it != entries_.end()) {
      entries_.erase(it);
    }
  }

  void erase(Entry &entry) { erase({std::addressof(entry)}); }

  void erase(Entry &entry, ::dingo::none_t) { erase(entry); }

  Entry *find_singular(bool &ambiguous) const {
    if (entries_.empty()) {
      ambiguous = false;
      return nullptr;
    }
    ambiguous = entries_.size() > 1;
    return ambiguous ? nullptr : entries_.front();
  }

  Entry *find_singular(::dingo::none_t, bool &ambiguous) const {
    return find_singular(ambiguous);
  }

  template <typename Fn> std::size_t for_each(Fn &&fn) const {
    for (auto *entry : entries_) {
      fn(*entry);
    }
    return entries_.size();
  }

  template <typename Fn> std::size_t for_each(::dingo::none_t, Fn &&fn) const {
    return for_each(std::forward<Fn>(fn));
  }

private:
  entries_type entries_;
};

template <typename Interface, typename Backend, typename Entry,
          typename Allocator>
class slot_storage<
    lookup_entry<Interface, ::dingo::no_key, ::dingo::one, Backend>, Entry,
    Allocator>
    : public static_key_slot_storage<Entry, Allocator, slot_cardinality::one> {
  using base_type =
      static_key_slot_storage<Entry, Allocator, slot_cardinality::one>;

public:
  using base_type::base_type;
};

template <typename Interface, typename Backend, typename Entry,
          typename Allocator>
class slot_storage<
    lookup_entry<Interface, ::dingo::no_key, ::dingo::many, Backend>, Entry,
    Allocator>
    : public static_key_slot_storage<Entry, Allocator, slot_cardinality::many> {
  using base_type =
      static_key_slot_storage<Entry, Allocator, slot_cardinality::many>;

public:
  using base_type::base_type;
};

template <typename Interface, typename Key, typename Backend, typename Entry,
          typename Allocator>
class slot_storage<
    lookup_entry<Interface, ::dingo::typed_key<Key>, ::dingo::one, Backend>,
    Entry, Allocator>
    : public static_key_slot_storage<Entry, Allocator, slot_cardinality::one> {
  using base_type =
      static_key_slot_storage<Entry, Allocator, slot_cardinality::one>;

public:
  using base_type::base_type;
};

template <typename Interface, typename Key, typename Backend, typename Entry,
          typename Allocator>
class slot_storage<
    lookup_entry<Interface, ::dingo::typed_key<Key>, ::dingo::many, Backend>,
    Entry, Allocator>
    : public static_key_slot_storage<Entry, Allocator, slot_cardinality::many> {
  using base_type =
      static_key_slot_storage<Entry, Allocator, slot_cardinality::many>;

public:
  using base_type::base_type;
};

template <typename Value>
auto lookup_backend_mapped(Value &value, int) -> decltype((value.second)) {
  return value.second;
}

template <typename Value> Value &lookup_backend_mapped(Value &value, ...) {
  return value;
}

template <typename Iterator>
decltype(auto) lookup_backend_iterator_mapped(Iterator iterator) {
  return lookup_backend_mapped(*iterator, 0);
}

template <typename Key, typename Entry, typename Cardinality, typename Backend,
          typename Allocator>
class runtime_key_slot_storage;

template <typename Key, typename Entry, typename Backend, typename Allocator>
class runtime_key_slot_storage<Key, Entry, ::dingo::one, Backend, Allocator> {
  using mapped_type = Entry *;
  using storage_type =
      typename Backend::template storage<Key, mapped_type, ::dingo::one,
                                         Allocator>;

public:
  using insertion_handle = typename storage_type::iterator;

  struct insertion_result {
    bool inserted;
    insertion_handle handle;

    explicit operator bool() const { return inserted; }
  };

  explicit runtime_key_slot_storage(Allocator &allocator)
      : storage_(allocator) {}

  template <typename KeyValue>
  bool conflicts(const KeyValue &key_value, const Entry &) const {
    return storage_.find(key_value) != storage_.end();
  }

  template <typename KeyValue>
  insertion_result insert(KeyValue &&key_value, Entry &entry) {
    auto [it, inserted] = storage_.try_emplace(
        std::forward<KeyValue>(key_value), std::addressof(entry));
    return {inserted, it};
  }

  void erase(insertion_handle handle) { storage_.erase(handle); }

  template <typename KeyValue>
  Entry *find_singular(const KeyValue &key_value, bool &ambiguous) const {
    auto it = storage_.find(key_value);
    ambiguous = false;
    return it == storage_.end() ? nullptr : lookup_backend_iterator_mapped(it);
  }

  template <typename KeyValue, typename Fn>
  std::size_t for_each(const KeyValue &key_value, Fn &&fn) const {
    auto it = storage_.find(key_value);
    if (it == storage_.end()) {
      return 0;
    }
    fn(*lookup_backend_iterator_mapped(it));
    return 1;
  }

private:
  storage_type storage_;
};

template <typename Key, typename Entry, typename Backend, typename Allocator>
class runtime_key_slot_storage<Key, Entry, ::dingo::many, Backend, Allocator> {
  using mapped_type = Entry *;
  using storage_type =
      typename Backend::template storage<Key, mapped_type, ::dingo::many,
                                         Allocator>;

public:
  using insertion_handle = typename storage_type::iterator;

  struct insertion_result {
    bool inserted;
    insertion_handle handle;

    explicit operator bool() const { return inserted; }
  };

  explicit runtime_key_slot_storage(Allocator &allocator)
      : storage_(allocator) {}

  template <typename KeyValue>
  bool conflicts(const KeyValue &, const Entry &) const {
    return false;
  }

  template <typename KeyValue>
  insertion_result insert(KeyValue &&key_value, Entry &entry) {
    return {true, storage_.emplace(std::forward<KeyValue>(key_value),
                                   std::addressof(entry))};
  }

  void erase(insertion_handle handle) { storage_.erase(handle); }

  template <typename KeyValue>
  Entry *find_singular(const KeyValue &key_value, bool &ambiguous) const {
    auto [first, last] = storage_.equal_range(key_value);
    if (first == last) {
      ambiguous = false;
      return nullptr;
    }
    auto next = first;
    ++next;
    ambiguous = next != last;
    return ambiguous ? nullptr : lookup_backend_iterator_mapped(first);
  }

  template <typename KeyValue, typename Fn>
  std::size_t for_each(const KeyValue &key_value, Fn &&fn) const {
    std::size_t result = 0;
    auto [first, last] = storage_.equal_range(key_value);
    for (auto it = first; it != last; ++it) {
      fn(*lookup_backend_iterator_mapped(it));
      ++result;
    }
    return result;
  }

private:
  storage_type storage_;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Backend, typename Entry, typename Allocator>
class slot_storage<
    lookup_entry<Interface, ::dingo::runtime_key<Key>, Cardinality, Backend>,
    Entry, Allocator> {
  using key_type = Key;
  static constexpr auto lookup_cardinality =
      slot_cardinality_value<Cardinality>();
  using storage_type = runtime_key_slot_storage<key_type, Entry, Cardinality,
                                                Backend, Allocator>;

public:
  using insertion_handle = typename storage_type::insertion_handle;
  using insertion_result = typename storage_type::insertion_result;

  explicit slot_storage(Allocator &allocator) : storage_(allocator) {}

  template <typename KeyValue>
  bool conflicts(const Entry &entry, const KeyValue &key_value) const {
    if (!matches(entry.key)) {
      return false;
    }
    return storage_.conflicts(key_value, entry);
  }

  template <typename KeyValue>
  insertion_result insert(Entry &entry, KeyValue &&key_value) {
    if (!matches(entry.key)) {
      return {false, {}};
    }
    return storage_.insert(std::forward<KeyValue>(key_value), entry);
  }

  void erase(insertion_handle handle) { storage_.erase(handle); }

  template <typename KeyValue>
  Entry *find_singular(const KeyValue &key_value, bool &ambiguous) const {
    return storage_.find_singular(key_value, ambiguous);
  }

  template <typename KeyValue, typename Fn>
  std::size_t for_each(const KeyValue &key_value, Fn &&fn) const {
    return storage_.for_each(key_value, std::forward<Fn>(fn));
  }

private:
  template <typename SlotKey> static bool matches(const SlotKey &key) {
    return key.domain == slot_domain::runtime_key &&
           key.cardinality == lookup_cardinality && key.key_type;
  }

  storage_type storage_;
};

template <typename Rtti> struct slot_key {
  using type_index = typename Rtti::type_index;

  type_index interface_type;
  slot_domain domain;
  slot_cardinality cardinality;
  std::optional<type_index> key_type;
};

template <typename Rtti, typename Interface, typename KeyDomain,
          typename Cardinality>
struct make_slot_key_impl;

template <typename Rtti, typename Interface, typename Cardinality>
struct make_slot_key_impl<Rtti, Interface, ::dingo::no_key, Cardinality> {
  static slot_key<Rtti> make() {
    return {Rtti::template get_type_index<Interface>(), slot_domain::no_key,
            slot_cardinality_value<Cardinality>(), std::nullopt};
  }
};

template <typename Rtti, typename Interface, typename Key, typename Cardinality>
struct make_slot_key_impl<Rtti, Interface, ::dingo::typed_key<Key>,
                          Cardinality> {
  static slot_key<Rtti> make() {
    return {Rtti::template get_type_index<Interface>(), slot_domain::typed_key,
            slot_cardinality_value<Cardinality>(),
            Rtti::template get_type_index<Key>()};
  }
};

template <typename Rtti, typename Interface, typename Key, typename Cardinality>
struct make_slot_key_impl<Rtti, Interface, ::dingo::runtime_key<Key>,
                          Cardinality> {
  static slot_key<Rtti> make() {
    using stored_key_type = std::decay_t<Key>;
    return {Rtti::template get_type_index<Interface>(),
            slot_domain::runtime_key, slot_cardinality_value<Cardinality>(),
            Rtti::template get_type_index<stored_key_type>()};
  }
};

template <typename Rtti, typename Interface, typename KeyDomain,
          typename Cardinality>
slot_key<Rtti> make_slot_key() {
  return make_slot_key_impl<Rtti, Interface, KeyDomain, Cardinality>::make();
}

} // namespace dingo::detail
