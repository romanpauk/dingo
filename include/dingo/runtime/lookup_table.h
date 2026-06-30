//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/index/index.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/type/dynamic_identity_map.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::detail {

enum class runtime_lookup_key_domain { no_key, typed_key, runtime_key };
enum class runtime_lookup_cardinality { one, many };

template <typename Cardinality>
constexpr runtime_lookup_cardinality runtime_lookup_cardinality_value() {
  if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
    return runtime_lookup_cardinality::one;
  } else {
    return runtime_lookup_cardinality::many;
  }
}

template <typename IndexEntry, bool = std::is_void_v<IndexEntry>>
struct runtime_lookup_entry_cardinality {
  using type = typename IndexEntry::cardinality;
};

template <typename IndexEntry>
struct runtime_lookup_entry_cardinality<IndexEntry, true> {
  using type = ::dingo::one;
};

template <typename Rtti, typename Allocator, typename Entry>
class runtime_lookup_table {
  struct erased_runtime_lookup_key {
    virtual ~erased_runtime_lookup_key() = default;
    virtual bool equals(const erased_runtime_lookup_key &other) const = 0;
  };

  template <typename Key>
  struct runtime_lookup_key_value : erased_runtime_lookup_key {
    template <typename KeyArg>
    explicit runtime_lookup_key_value(KeyArg &&key_arg)
        : value(std::forward<KeyArg>(key_arg)) {}

    bool equals(const erased_runtime_lookup_key &other) const override {
      auto *typed = dynamic_cast<const runtime_lookup_key_value *>(&other);
      return typed && typed->value == value;
    }

    Key value;
  };

  template <typename Key>
  using runtime_lookup_key_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>,
      std::allocator<runtime_lookup_key_value<Key>>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          runtime_lookup_key_value<Key>>>;

public:
  using key_domain = runtime_lookup_key_domain;
  using cardinality = runtime_lookup_cardinality;
  using type_index = typename Rtti::type_index;

  struct runtime_lookup_key_owner {
    runtime_lookup_key_owner() = default;

    ~runtime_lookup_key_owner() { reset(); }

    runtime_lookup_key_owner(const runtime_lookup_key_owner &) = delete;
    runtime_lookup_key_owner &
    operator=(const runtime_lookup_key_owner &) = delete;

    runtime_lookup_key_owner(runtime_lookup_key_owner &&other) noexcept
        : allocator_(other.allocator_), ptr_(other.ptr_),
          destroy_(other.destroy_) {
      other.allocator_ = nullptr;
      other.ptr_ = nullptr;
      other.destroy_ = nullptr;
    }

    runtime_lookup_key_owner &
    operator=(runtime_lookup_key_owner &&other) noexcept {
      if (this != &other) {
        reset();
        allocator_ = other.allocator_;
        ptr_ = other.ptr_;
        destroy_ = other.destroy_;
        other.allocator_ = nullptr;
        other.ptr_ = nullptr;
        other.destroy_ = nullptr;
      }
      return *this;
    }

    explicit operator bool() const { return ptr_ != nullptr; }

    erased_runtime_lookup_key *get() const { return ptr_; }
    erased_runtime_lookup_key &operator*() const { return *ptr_; }
    erased_runtime_lookup_key *operator->() const { return ptr_; }

    template <typename Key, typename KeyValue>
    static runtime_lookup_key_owner make(Allocator &allocator,
                                         KeyValue &&key_value) {
      using stored_key_type = std::decay_t<Key>;
      using concrete_allocator = runtime_lookup_key_allocator<stored_key_type>;
      using concrete_allocator_traits =
          std::allocator_traits<concrete_allocator>;

      auto concrete_alloc =
          make_lookup_storage_allocator<concrete_allocator>(allocator);
      auto *ptr = concrete_allocator_traits::allocate(concrete_alloc, 1);
      try {
        concrete_allocator_traits::construct(concrete_alloc, ptr,
                                             std::forward<KeyValue>(key_value));
      } catch (...) {
        concrete_allocator_traits::deallocate(concrete_alloc, ptr, 1);
        throw;
      }

      runtime_lookup_key_owner owner;
      owner.allocator_ = std::addressof(allocator);
      owner.ptr_ = ptr;
      owner.destroy_ = &destroy_key<stored_key_type>;
      return owner;
    }

  private:
    template <typename Key>
    static void destroy_key(Allocator &allocator,
                            erased_runtime_lookup_key *ptr) noexcept {
      using concrete_type = runtime_lookup_key_value<Key>;
      using concrete_allocator = runtime_lookup_key_allocator<Key>;
      using concrete_allocator_traits =
          std::allocator_traits<concrete_allocator>;

      auto concrete_alloc =
          make_lookup_storage_allocator<concrete_allocator>(allocator);
      auto *typed_ptr = static_cast<concrete_type *>(ptr);
      concrete_allocator_traits::destroy(concrete_alloc, typed_ptr);
      concrete_allocator_traits::deallocate(concrete_alloc, typed_ptr, 1);
    }

    void reset() noexcept {
      if (ptr_) {
        destroy_(*allocator_, ptr_);
        allocator_ = nullptr;
        ptr_ = nullptr;
        destroy_ = nullptr;
      }
    }

    Allocator *allocator_ = nullptr;
    erased_runtime_lookup_key *ptr_ = nullptr;
    void (*destroy_)(Allocator &,
                     erased_runtime_lookup_key *) noexcept = nullptr;
  };

  struct lookup_identity {
    type_index interface_type;
    key_domain domain;
    cardinality lookup_cardinality;
    std::optional<type_index> key_type;
    runtime_lookup_key_owner runtime_key;
    bool enforces_lookup_duplicates;

    lookup_identity(type_index resolved_interface_type,
                    key_domain resolved_domain,
                    cardinality resolved_cardinality,
                    std::optional<type_index> resolved_key_type,
                    runtime_lookup_key_owner &&resolved_runtime_key =
                        runtime_lookup_key_owner{},
                    bool resolved_enforces_lookup_duplicates = true)
        : interface_type(std::move(resolved_interface_type)),
          domain(resolved_domain), lookup_cardinality(resolved_cardinality),
          key_type(std::move(resolved_key_type)),
          runtime_key(std::move(resolved_runtime_key)),
          enforces_lookup_duplicates(resolved_enforces_lookup_duplicates) {}

    lookup_identity(const lookup_identity &) = delete;
    lookup_identity &operator=(const lookup_identity &) = delete;
    lookup_identity(lookup_identity &&) noexcept = default;
    lookup_identity &operator=(lookup_identity &&) noexcept = default;
  };

  using identity_allocator =
      typename std::conditional_t<::dingo::is_static_allocator_v<Allocator>,
                                  std::allocator<lookup_identity>,
                                  typename std::allocator_traits<Allocator>::
                                      template rebind_alloc<lookup_identity>>;
  using identity_storage = std::vector<lookup_identity, identity_allocator>;

  using row_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>, std::allocator<Entry *>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          Entry *>>;
  using row_storage = std::vector<Entry *, row_allocator>;

  struct bucket_key {
    type_index interface_type;
    key_domain domain;
    cardinality lookup_cardinality;
    std::optional<type_index> key_type;
  };

  struct bucket_key_compare {
    bool operator()(const bucket_key &lhs, const bucket_key &rhs) const {
      std::less<type_index> less;
      if (less(lhs.interface_type, rhs.interface_type)) {
        return true;
      }
      if (less(rhs.interface_type, lhs.interface_type)) {
        return false;
      }
      if (lhs.domain != rhs.domain) {
        return lhs.domain < rhs.domain;
      }
      if (lhs.lookup_cardinality != rhs.lookup_cardinality) {
        return lhs.lookup_cardinality < rhs.lookup_cardinality;
      }
      if (lhs.key_type && rhs.key_type) {
        return less(*lhs.key_type, *rhs.key_type);
      }
      return lhs.key_type.has_value() < rhs.key_type.has_value();
    }
  };

  using bucket_map = dynamic_identity_map<bucket_key, row_storage, Allocator,
                                          bucket_key_compare>;

  explicit runtime_lookup_table(Allocator &allocator)
      : allocator_(std::addressof(allocator)), buckets_(allocator) {}

  template <typename Interface, typename Cardinality>
  static lookup_identity make_no_key_identity() {
    return lookup_identity{
        Rtti::template get_type_index<Interface>(), key_domain::no_key,
        runtime_lookup_cardinality_value<Cardinality>(), std::nullopt};
  }

  template <typename Interface, typename Key, typename Cardinality>
  static lookup_identity make_typed_key_identity() {
    return lookup_identity{Rtti::template get_type_index<Interface>(),
                           key_domain::typed_key,
                           runtime_lookup_cardinality_value<Cardinality>(),
                           Rtti::template get_type_index<Key>()};
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename KeyValue>
  static lookup_identity make_runtime_key_identity(Allocator &allocator,
                                                   KeyValue &&key_value) {
    using stored_key_type = std::decay_t<Key>;
    auto erased_key = runtime_lookup_key_owner::template make<stored_key_type>(
        allocator, std::forward<KeyValue>(key_value));
    return lookup_identity{Rtti::template get_type_index<Interface>(),
                           key_domain::runtime_key,
                           runtime_lookup_cardinality_value<Cardinality>(),
                           Rtti::template get_type_index<stored_key_type>(),
                           std::move(erased_key)};
  }

  bool insert(Entry &entry) {
    for (const auto &identity : entry.lookup_identities) {
      if (conflicts(entry, identity)) {
        return false;
      }
    }

    try {
      for (const auto &identity : entry.lookup_identities) {
        auto &rows = bucket(identity);
        rows.emplace_back(std::addressof(entry));
      }
    } catch (...) {
      erase(entry);
      throw;
    }

    return true;
  }

  void erase(Entry &entry) {
    auto *entry_ptr = std::addressof(entry);
    for (auto it = buckets_.begin(); it != buckets_.end();) {
      auto &rows = it->second;
      rows.erase(std::remove(rows.begin(), rows.end(), entry_ptr), rows.end());
      if (rows.empty()) {
        it = buckets_.erase(it);
      } else {
        ++it;
      }
    }
  }

  template <typename Interface, typename Cardinality>
  Entry *find_singular_no_key(bool &ambiguous) const {
    return find_singular_in_bucket(
        make_no_key_bucket<Interface, Cardinality>(), ambiguous,
        [](const auto &identity) {
          return matches_no_key<Interface, Cardinality>(identity);
        });
  }

  template <typename Interface, typename Key, typename Cardinality>
  Entry *find_singular_typed_key(bool &ambiguous) const {
    return find_singular_in_bucket(
        make_typed_key_bucket<Interface, Key, Cardinality>(), ambiguous,
        [](const auto &identity) {
          return matches_typed_key<Interface, Key, Cardinality>(identity);
        });
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename KeyValue>
  Entry *find_singular_runtime_key(const KeyValue &key_value,
                                   bool &ambiguous) const {
    return find_singular_in_bucket(
        make_runtime_key_bucket<Interface, Key, Cardinality>(), ambiguous,
        [&](const auto &identity) {
          return matches_runtime_key<Interface, Key, Cardinality>(identity,
                                                                  key_value);
        });
  }

  template <typename Interface, typename Cardinality, typename Fn>
  std::size_t for_each_no_key(Fn &&fn) const {
    return for_each_in_bucket(
        make_no_key_bucket<Interface, Cardinality>(),
        [](const auto &identity) {
          return matches_no_key<Interface, Cardinality>(identity);
        },
        std::forward<Fn>(fn));
  }

  template <typename Interface, typename Key, typename Cardinality, typename Fn>
  std::size_t for_each_typed_key(Fn &&fn) const {
    return for_each_in_bucket(
        make_typed_key_bucket<Interface, Key, Cardinality>(),
        [](const auto &identity) {
          return matches_typed_key<Interface, Key, Cardinality>(identity);
        },
        std::forward<Fn>(fn));
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename KeyValue, typename Fn>
  std::size_t for_each_runtime_key(const KeyValue &key_value, Fn &&fn) const {
    return for_each_in_bucket(
        make_runtime_key_bucket<Interface, Key, Cardinality>(),
        [&](const auto &identity) {
          return matches_runtime_key<Interface, Key, Cardinality>(identity,
                                                                  key_value);
        },
        std::forward<Fn>(fn));
  }

  template <typename Interface, typename Cardinality>
  std::size_t count_no_key() const {
    return for_each_no_key<Interface, Cardinality>([](const Entry &) {});
  }

  template <typename Interface, typename Key, typename Cardinality>
  std::size_t count_typed_key() const {
    return for_each_typed_key<Interface, Key, Cardinality>(
        [](const Entry &) {});
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename KeyValue>
  std::size_t count_runtime_key(const KeyValue &key_value) const {
    return for_each_runtime_key<Interface, Key, Cardinality>(
        key_value, [](const Entry &) {});
  }

  template <typename Interface, typename Cardinality>
  static bool matches_no_key(const lookup_identity &identity) {
    return identity.interface_type ==
               Rtti::template get_type_index<Interface>() &&
           identity.domain == key_domain::no_key &&
           identity.lookup_cardinality ==
               runtime_lookup_cardinality_value<Cardinality>() &&
           !identity.key_type && !identity.runtime_key;
  }

  template <typename Interface, typename Key, typename Cardinality>
  static bool matches_typed_key(const lookup_identity &identity) {
    return identity.interface_type ==
               Rtti::template get_type_index<Interface>() &&
           identity.domain == key_domain::typed_key &&
           identity.lookup_cardinality ==
               runtime_lookup_cardinality_value<Cardinality>() &&
           identity.key_type &&
           *identity.key_type == Rtti::template get_type_index<Key>() &&
           !identity.runtime_key;
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename KeyValue>
  static bool matches_runtime_key(const lookup_identity &identity,
                                  const KeyValue &key_value) {
    using stored_key_type = std::decay_t<Key>;
    if (!(identity.interface_type ==
              Rtti::template get_type_index<Interface>() &&
          identity.domain == key_domain::runtime_key &&
          identity.lookup_cardinality ==
              runtime_lookup_cardinality_value<Cardinality>() &&
          identity.key_type &&
          *identity.key_type ==
              Rtti::template get_type_index<stored_key_type>() &&
          identity.runtime_key)) {
      return false;
    }

    auto *stored_key = static_cast<runtime_lookup_key_value<stored_key_type> *>(
        identity.runtime_key.get());
    return stored_key->value == key_value;
  }

  template <typename Matcher>
  static bool entry_matches(Entry &entry, Matcher &matches) {
    for (const auto &identity : entry.lookup_identities) {
      if (matches(identity)) {
        return true;
      }
    }
    return false;
  }

  static bool identity_base_matches(const lookup_identity &lhs,
                                    const lookup_identity &rhs) {
    return lhs.interface_type == rhs.interface_type &&
           lhs.domain == rhs.domain &&
           lhs.lookup_cardinality == rhs.lookup_cardinality &&
           lhs.key_type == rhs.key_type;
  }

  static bool identity_lookup_value_matches(const lookup_identity &lhs,
                                            const lookup_identity &rhs) {
    if (!identity_base_matches(lhs, rhs)) {
      return false;
    }
    if (lhs.domain != key_domain::runtime_key) {
      return true;
    }
    return lhs.runtime_key && rhs.runtime_key &&
           lhs.runtime_key->equals(*rhs.runtime_key);
  }

private:
  static bucket_key make_bucket(const lookup_identity &identity) {
    return {identity.interface_type, identity.domain,
            identity.lookup_cardinality, identity.key_type};
  }

  template <typename Interface, typename Cardinality>
  static bucket_key make_no_key_bucket() {
    return {Rtti::template get_type_index<Interface>(), key_domain::no_key,
            runtime_lookup_cardinality_value<Cardinality>(), std::nullopt};
  }

  template <typename Interface, typename Key, typename Cardinality>
  static bucket_key make_typed_key_bucket() {
    return {Rtti::template get_type_index<Interface>(), key_domain::typed_key,
            runtime_lookup_cardinality_value<Cardinality>(),
            Rtti::template get_type_index<Key>()};
  }

  template <typename Interface, typename Key, typename Cardinality>
  static bucket_key make_runtime_key_bucket() {
    using stored_key_type = std::decay_t<Key>;
    return {Rtti::template get_type_index<Interface>(), key_domain::runtime_key,
            runtime_lookup_cardinality_value<Cardinality>(),
            Rtti::template get_type_index<stored_key_type>()};
  }

  row_storage &bucket(const lookup_identity &identity) {
    auto resolved_bucket = make_bucket(identity);
    auto rows = buckets_.get(resolved_bucket);
    if (rows) {
      return *rows;
    }
    return buckets_
        .insert(std::move(resolved_bucket),
                make_lookup_storage_allocator<row_allocator>(*allocator_))
        .first;
  }

  template <typename Matches>
  Entry *find_singular_in_bucket(const bucket_key &resolved_bucket,
                                 bool &ambiguous, Matches &&matches) const {
    Entry *selected = nullptr;
    std::size_t match_count = 0;
    auto *rows = buckets_.get(resolved_bucket);
    if (rows) {
      for (auto *entry : *rows) {
        if (!entry_matches(*entry, matches)) {
          continue;
        }
        ++match_count;
        if (match_count == 1) {
          selected = entry;
        }
      }
    }
    ambiguous = match_count > 1;
    return ambiguous ? nullptr : selected;
  }

  template <typename Matches, typename Fn>
  std::size_t for_each_in_bucket(const bucket_key &resolved_bucket,
                                 Matches &&matches, Fn &&fn) const {
    std::size_t count = 0;
    auto *rows = buckets_.get(resolved_bucket);
    if (rows) {
      for (auto *entry : *rows) {
        if (entry_matches(*entry, matches)) {
          ++count;
          fn(*entry);
        }
      }
    }
    return count;
  }

  bool conflicts(const Entry &entry, const lookup_identity &identity) const {
    auto *rows = buckets_.get(make_bucket(identity));
    if (!rows) {
      return false;
    }
    for (auto *existing_entry : *rows) {
      for (const auto &existing_identity : existing_entry->lookup_identities) {
        if (!identity_lookup_value_matches(identity, existing_identity)) {
          continue;
        }
        if (!identity.enforces_lookup_duplicates ||
            !existing_identity.enforces_lookup_duplicates) {
          continue;
        }
        if (identity.lookup_cardinality == cardinality::one ||
            existing_entry->storage_type == entry.storage_type) {
          return true;
        }
      }
    }
    return false;
  }

  Allocator *allocator_;
  bucket_map buckets_;
};

} // namespace dingo::detail
