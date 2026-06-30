//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/index/index.h>
#include <dingo/memory/static_allocator.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo::detail {

enum class runtime_selector_key_domain { no_key, typed_key, runtime_key };
enum class runtime_selector_cardinality { one, many };

template <typename Cardinality>
constexpr runtime_selector_cardinality runtime_selector_cardinality_value() {
  if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
    return runtime_selector_cardinality::one;
  } else {
    return runtime_selector_cardinality::many;
  }
}

template <typename IndexEntry, bool = std::is_void_v<IndexEntry>>
struct runtime_selector_entry_cardinality {
  using type = typename IndexEntry::cardinality;
};

template <typename IndexEntry>
struct runtime_selector_entry_cardinality<IndexEntry, true> {
  using type = ::dingo::one;
};

template <typename Rtti, typename Allocator, typename Entry>
class runtime_selector_table {
  struct erased_runtime_selector_key {
    virtual ~erased_runtime_selector_key() = default;
    virtual bool equals(const erased_runtime_selector_key &other) const = 0;
  };

  template <typename Key>
  struct runtime_selector_key_value : erased_runtime_selector_key {
    template <typename KeyArg>
    explicit runtime_selector_key_value(KeyArg &&key_arg)
        : value(std::forward<KeyArg>(key_arg)) {}

    bool equals(const erased_runtime_selector_key &other) const override {
      auto *typed = dynamic_cast<const runtime_selector_key_value *>(&other);
      return typed && typed->value == value;
    }

    Key value;
  };

  template <typename Key>
  using runtime_selector_key_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>,
      std::allocator<runtime_selector_key_value<Key>>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          runtime_selector_key_value<Key>>>;

public:
  using key_domain = runtime_selector_key_domain;
  using cardinality = runtime_selector_cardinality;
  using type_index = typename Rtti::type_index;

  struct runtime_selector_key_owner {
    runtime_selector_key_owner() = default;

    ~runtime_selector_key_owner() { reset(); }

    runtime_selector_key_owner(const runtime_selector_key_owner &) = delete;
    runtime_selector_key_owner &
    operator=(const runtime_selector_key_owner &) = delete;

    runtime_selector_key_owner(runtime_selector_key_owner &&other) noexcept
        : allocator_(other.allocator_), ptr_(other.ptr_),
          destroy_(other.destroy_) {
      other.allocator_ = nullptr;
      other.ptr_ = nullptr;
      other.destroy_ = nullptr;
    }

    runtime_selector_key_owner &
    operator=(runtime_selector_key_owner &&other) noexcept {
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

    erased_runtime_selector_key *get() const { return ptr_; }
    erased_runtime_selector_key &operator*() const { return *ptr_; }
    erased_runtime_selector_key *operator->() const { return ptr_; }

    template <typename Key, typename KeyValue>
    static runtime_selector_key_owner make(Allocator &allocator,
                                           KeyValue &&key_value) {
      using stored_key_type = std::decay_t<Key>;
      using concrete_allocator =
          runtime_selector_key_allocator<stored_key_type>;
      using concrete_allocator_traits =
          std::allocator_traits<concrete_allocator>;

      auto concrete_alloc =
          make_selector_storage_allocator<concrete_allocator>(allocator);
      auto *ptr = concrete_allocator_traits::allocate(concrete_alloc, 1);
      try {
        concrete_allocator_traits::construct(concrete_alloc, ptr,
                                             std::forward<KeyValue>(key_value));
      } catch (...) {
        concrete_allocator_traits::deallocate(concrete_alloc, ptr, 1);
        throw;
      }

      runtime_selector_key_owner owner;
      owner.allocator_ = std::addressof(allocator);
      owner.ptr_ = ptr;
      owner.destroy_ = &destroy_key<stored_key_type>;
      return owner;
    }

  private:
    template <typename Key>
    static void destroy_key(Allocator &allocator,
                            erased_runtime_selector_key *ptr) noexcept {
      using concrete_type = runtime_selector_key_value<Key>;
      using concrete_allocator = runtime_selector_key_allocator<Key>;
      using concrete_allocator_traits =
          std::allocator_traits<concrete_allocator>;

      auto concrete_alloc =
          make_selector_storage_allocator<concrete_allocator>(allocator);
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
    erased_runtime_selector_key *ptr_ = nullptr;
    void (*destroy_)(Allocator &,
                     erased_runtime_selector_key *) noexcept = nullptr;
  };

  struct selector_identity {
    type_index interface_type;
    key_domain domain;
    cardinality selector_cardinality;
    std::optional<type_index> key_type;
    runtime_selector_key_owner runtime_key;
    bool enforces_selector_duplicates;

    selector_identity(type_index resolved_interface_type,
                      key_domain resolved_domain,
                      cardinality resolved_cardinality,
                      std::optional<type_index> resolved_key_type,
                      runtime_selector_key_owner &&resolved_runtime_key =
                          runtime_selector_key_owner{},
                      bool resolved_enforces_selector_duplicates = true)
        : interface_type(std::move(resolved_interface_type)),
          domain(resolved_domain), selector_cardinality(resolved_cardinality),
          key_type(std::move(resolved_key_type)),
          runtime_key(std::move(resolved_runtime_key)),
          enforces_selector_duplicates(resolved_enforces_selector_duplicates) {}

    selector_identity(const selector_identity &) = delete;
    selector_identity &operator=(const selector_identity &) = delete;
    selector_identity(selector_identity &&) noexcept = default;
    selector_identity &operator=(selector_identity &&) noexcept = default;
  };

  using identity_allocator =
      typename std::conditional_t<::dingo::is_static_allocator_v<Allocator>,
                                  std::allocator<selector_identity>,
                                  typename std::allocator_traits<Allocator>::
                                      template rebind_alloc<selector_identity>>;
  using identity_storage = std::vector<selector_identity, identity_allocator>;

  using row_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>, std::allocator<Entry *>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          Entry *>>;
  using row_storage = std::vector<Entry *, row_allocator>;

  explicit runtime_selector_table(Allocator &allocator)
      : rows_(make_selector_storage_allocator<row_allocator>(allocator)) {}

  template <typename Interface, typename Cardinality>
  static selector_identity make_no_key_identity() {
    return selector_identity{
        Rtti::template get_type_index<Interface>(), key_domain::no_key,
        runtime_selector_cardinality_value<Cardinality>(), std::nullopt};
  }

  template <typename Interface, typename Cardinality>
  static selector_identity make_legacy_no_key_identity() {
    return selector_identity{Rtti::template get_type_index<Interface>(),
                             key_domain::no_key,
                             runtime_selector_cardinality_value<Cardinality>(),
                             std::nullopt,
                             runtime_selector_key_owner{},
                             false};
  }

  template <typename Interface, typename Key, typename Cardinality>
  static selector_identity make_typed_key_identity() {
    return selector_identity{Rtti::template get_type_index<Interface>(),
                             key_domain::typed_key,
                             runtime_selector_cardinality_value<Cardinality>(),
                             Rtti::template get_type_index<Key>()};
  }

  template <typename Interface, typename Key, typename Cardinality>
  static selector_identity make_legacy_typed_key_identity() {
    return selector_identity{Rtti::template get_type_index<Interface>(),
                             key_domain::typed_key,
                             runtime_selector_cardinality_value<Cardinality>(),
                             Rtti::template get_type_index<Key>(),
                             runtime_selector_key_owner{},
                             false};
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename KeyValue>
  static selector_identity make_runtime_key_identity(Allocator &allocator,
                                                     KeyValue &&key_value) {
    using stored_key_type = std::decay_t<Key>;
    auto erased_key =
        runtime_selector_key_owner::template make<stored_key_type>(
            allocator, std::forward<KeyValue>(key_value));
    return selector_identity{Rtti::template get_type_index<Interface>(),
                             key_domain::runtime_key,
                             runtime_selector_cardinality_value<Cardinality>(),
                             Rtti::template get_type_index<stored_key_type>(),
                             std::move(erased_key)};
  }

  bool insert(Entry &entry) {
    for (const auto &identity : entry.selector_identities) {
      if (conflicts(entry, identity)) {
        return false;
      }
    }

    if (entry.selector_identities.empty()) {
      return true;
    }

    try {
      rows_.emplace_back(std::addressof(entry));
    } catch (...) {
      erase(entry);
      throw;
    }

    return true;
  }

  void erase(Entry &entry) {
    auto *entry_ptr = std::addressof(entry);
    rows_.erase(std::remove(rows_.begin(), rows_.end(), entry_ptr),
                rows_.end());
  }

  template <typename Matcher> Entry *find_singular(Matcher &&matches) const {
    bool ambiguous = false;
    return find_singular(std::forward<Matcher>(matches), ambiguous);
  }

  template <typename Matcher>
  Entry *find_singular(Matcher &&matches, bool &ambiguous) const {
    Entry *selected = nullptr;
    std::size_t match_count = 0;
    for (auto *entry : rows_) {
      if (!entry_matches(*entry, matches)) {
        continue;
      }
      ++match_count;
      if (match_count == 1) {
        selected = entry;
      }
    }

    ambiguous = match_count > 1;
    return ambiguous ? nullptr : selected;
  }

  template <typename Matcher, typename Fn>
  std::size_t for_each(Matcher &&matches, Fn &&fn) const {
    std::size_t count = 0;
    for (auto *entry : rows_) {
      if (entry_matches(*entry, matches)) {
        ++count;
        fn(*entry);
      }
    }
    return count;
  }

  template <typename Matcher> std::size_t count(Matcher &&matches) const {
    return for_each(std::forward<Matcher>(matches), [](const Entry &) {});
  }

  template <typename Interface, typename Cardinality>
  static bool matches_no_key(const selector_identity &identity) {
    return identity.interface_type ==
               Rtti::template get_type_index<Interface>() &&
           identity.domain == key_domain::no_key &&
           identity.selector_cardinality ==
               runtime_selector_cardinality_value<Cardinality>() &&
           !identity.key_type && !identity.runtime_key;
  }

  template <typename Interface, typename Key, typename Cardinality>
  static bool matches_typed_key(const selector_identity &identity) {
    return identity.interface_type ==
               Rtti::template get_type_index<Interface>() &&
           identity.domain == key_domain::typed_key &&
           identity.selector_cardinality ==
               runtime_selector_cardinality_value<Cardinality>() &&
           identity.key_type &&
           *identity.key_type == Rtti::template get_type_index<Key>() &&
           !identity.runtime_key;
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename KeyValue>
  static bool matches_runtime_key(const selector_identity &identity,
                                  const KeyValue &key_value) {
    using stored_key_type = std::decay_t<Key>;
    if (!(identity.interface_type ==
              Rtti::template get_type_index<Interface>() &&
          identity.domain == key_domain::runtime_key &&
          identity.selector_cardinality ==
              runtime_selector_cardinality_value<Cardinality>() &&
          identity.key_type &&
          *identity.key_type ==
              Rtti::template get_type_index<stored_key_type>() &&
          identity.runtime_key)) {
      return false;
    }

    auto *stored_key =
        static_cast<runtime_selector_key_value<stored_key_type> *>(
            identity.runtime_key.get());
    return stored_key->value == key_value;
  }

  template <typename Matcher>
  static bool entry_matches(Entry &entry, Matcher &matches) {
    for (const auto &identity : entry.selector_identities) {
      if (matches(identity)) {
        return true;
      }
    }
    return false;
  }

  static bool identity_base_matches(const selector_identity &lhs,
                                    const selector_identity &rhs) {
    return lhs.interface_type == rhs.interface_type &&
           lhs.domain == rhs.domain &&
           lhs.selector_cardinality == rhs.selector_cardinality &&
           lhs.key_type == rhs.key_type;
  }

  static bool identity_lookup_value_matches(const selector_identity &lhs,
                                            const selector_identity &rhs) {
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
  bool conflicts(const Entry &entry, const selector_identity &identity) const {
    for (auto *existing_entry : rows_) {
      for (const auto &existing_identity :
           existing_entry->selector_identities) {
        if (!identity_lookup_value_matches(identity, existing_identity)) {
          continue;
        }
        if (!identity.enforces_selector_duplicates ||
            !existing_identity.enforces_selector_duplicates) {
          continue;
        }
        if (identity.selector_cardinality == cardinality::one ||
            existing_entry->storage_type == entry.storage_type) {
          return true;
        }
      }
    }
    return false;
  }

  row_storage rows_;
};

} // namespace dingo::detail
