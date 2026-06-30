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
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <unordered_map>
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

template <typename Key, typename Rows, typename Allocator>
class linear_lookup_storage {
  using value_type = std::pair<Key, Rows>;
  using storage_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>, std::allocator<value_type>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          value_type>>;
  using storage_type = std::vector<value_type, storage_allocator>;

public:
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  explicit linear_lookup_storage(Allocator &allocator)
      : allocator_(std::addressof(allocator)),
        values_(make_lookup_storage_allocator<storage_allocator>(allocator)) {}

  iterator find(const Key &key) {
    return std::find_if(values_.begin(), values_.end(),
                        [&](auto &value) { return value.first == key; });
  }

  const_iterator find(const Key &key) const {
    return std::find_if(values_.begin(), values_.end(),
                        [&](const auto &value) { return value.first == key; });
  }

  iterator end() { return values_.end(); }
  const_iterator end() const { return values_.end(); }
  iterator begin() { return values_.begin(); }
  const_iterator begin() const { return values_.begin(); }

  Rows &operator[](const Key &key) {
    auto it = find(key);
    if (it == values_.end()) {
      values_.emplace_back(
          key, make_lookup_storage_allocator<typename Rows::allocator_type>(
                   *allocator_));
      it = std::prev(values_.end());
    }
    return it->second;
  }

  iterator erase(iterator it) { return values_.erase(it); }

private:
  Allocator *allocator_;
  storage_type values_;
};

template <typename Key, typename Rows, typename Allocator>
class ordered_lookup_storage {
  using value_type = std::pair<Key, Rows>;
  using storage_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>, std::allocator<value_type>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          value_type>>;
  using storage_type = std::vector<value_type, storage_allocator>;

public:
  using iterator = typename storage_type::iterator;
  using const_iterator = typename storage_type::const_iterator;

  explicit ordered_lookup_storage(Allocator &allocator)
      : allocator_(std::addressof(allocator)),
        values_(make_lookup_storage_allocator<storage_allocator>(allocator)) {}

  iterator find(const Key &key) {
    auto it = lower_bound(key);
    return it != values_.end() && !less_(key, it->first) ? it : values_.end();
  }

  const_iterator find(const Key &key) const {
    auto it = lower_bound(key);
    return it != values_.end() && !less_(key, it->first) ? it : values_.end();
  }

  iterator end() { return values_.end(); }
  const_iterator end() const { return values_.end(); }
  iterator begin() { return values_.begin(); }
  const_iterator begin() const { return values_.begin(); }

  Rows &operator[](const Key &key) {
    auto it = lower_bound(key);
    if (it == values_.end() || less_(key, it->first)) {
      it = values_.emplace(
          it, key,
          make_lookup_storage_allocator<typename Rows::allocator_type>(
              *allocator_));
    }
    return it->second;
  }

  iterator erase(iterator it) { return values_.erase(it); }

private:
  iterator lower_bound(const Key &key) {
    return std::lower_bound(values_.begin(), values_.end(), key,
                            [&](const auto &value, const Key &resolved_key) {
                              return less_(value.first, resolved_key);
                            });
  }

  const_iterator lower_bound(const Key &key) const {
    return std::lower_bound(values_.begin(), values_.end(), key,
                            [&](const auto &value, const Key &resolved_key) {
                              return less_(value.first, resolved_key);
                            });
  }

  Allocator *allocator_;
  std::less<Key> less_;
  storage_type values_;
};

template <typename Key, typename Rows, typename Allocator>
class unordered_lookup_storage
    : public std::unordered_map<
          Key, Rows, std::hash<Key>, std::equal_to<Key>,
          typename std::conditional_t<
              ::dingo::is_static_allocator_v<Allocator>,
              std::allocator<std::pair<const Key, Rows>>,
              typename std::allocator_traits<Allocator>::template rebind_alloc<
                  std::pair<const Key, Rows>>>> {
  using base = std::unordered_map<
      Key, Rows, std::hash<Key>, std::equal_to<Key>,
      typename std::conditional_t<
          ::dingo::is_static_allocator_v<Allocator>,
          std::allocator<std::pair<const Key, Rows>>,
          typename std::allocator_traits<Allocator>::template rebind_alloc<
              std::pair<const Key, Rows>>>>;

public:
  explicit unordered_lookup_storage(Allocator &allocator)
      : base(0, std::hash<Key>{}, std::equal_to<Key>{},
             make_lookup_storage_allocator<typename base::allocator_type>(
                 allocator)),
        allocator_(std::addressof(allocator)) {}

  Rows &operator[](const Key &key) {
    auto it = this->find(key);
    if (it == this->end()) {
      it = this->emplace(std::piecewise_construct, std::forward_as_tuple(key),
                         std::forward_as_tuple(
                             make_lookup_storage_allocator<
                                 typename Rows::allocator_type>(*allocator_)))
               .first;
    }
    return it->second;
  }

private:
  Allocator *allocator_;
};

template <typename Key, bool IsEnum = std::is_enum_v<Key>>
struct array_lookup_index_type {
  using type = Key;
};

template <typename Key> struct array_lookup_index_type<Key, true> {
  using type = std::underlying_type_t<Key>;
};

template <typename Key, typename Rows, typename Allocator, std::size_t Size>
class array_lookup_storage {
  struct slot {
    explicit slot(Allocator &allocator)
        : second(make_lookup_storage_allocator<typename Rows::allocator_type>(
              allocator)) {}

    bool engaged = false;
    Rows second;
  };

  using slot_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>, std::allocator<slot>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<slot>>;
  using storage_type = std::vector<slot, slot_allocator>;

public:
  class iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = slot;
    using pointer = slot *;
    using reference = slot &;
    using iterator_category = std::forward_iterator_tag;

    iterator() = default;
    iterator(array_lookup_storage *storage, std::size_t index)
        : storage_(storage), index_(index) {}

    reference operator*() const { return storage_->values_[index_]; }
    pointer operator->() const { return std::addressof(**this); }
    iterator &operator++() {
      ++index_;
      skip_empty();
      return *this;
    }
    bool operator==(const iterator &other) const {
      return storage_ == other.storage_ && index_ == other.index_;
    }
    bool operator!=(const iterator &other) const { return !(*this == other); }

  private:
    friend class array_lookup_storage;

    void skip_empty() {
      while (index_ < Size && !storage_->values_[index_].engaged) {
        ++index_;
      }
    }

    array_lookup_storage *storage_ = nullptr;
    std::size_t index_ = Size;
  };

  class const_iterator {
  public:
    using difference_type = std::ptrdiff_t;
    using value_type = const slot;
    using pointer = const slot *;
    using reference = const slot &;
    using iterator_category = std::forward_iterator_tag;

    const_iterator() = default;
    const_iterator(const array_lookup_storage *storage, std::size_t index)
        : storage_(storage), index_(index) {}

    reference operator*() const { return storage_->values_[index_]; }
    pointer operator->() const { return std::addressof(**this); }
    const_iterator &operator++() {
      ++index_;
      skip_empty();
      return *this;
    }
    bool operator==(const const_iterator &other) const {
      return storage_ == other.storage_ && index_ == other.index_;
    }
    bool operator!=(const const_iterator &other) const {
      return !(*this == other);
    }

  private:
    friend class array_lookup_storage;

    void skip_empty() {
      while (index_ < Size && !storage_->values_[index_].engaged) {
        ++index_;
      }
    }

    const array_lookup_storage *storage_ = nullptr;
    std::size_t index_ = Size;
  };

  explicit array_lookup_storage(Allocator &allocator)
      : values_(make_lookup_storage_allocator<slot_allocator>(allocator)) {
    values_.reserve(Size);
    for (std::size_t index = 0; index < Size; ++index) {
      values_.emplace_back(allocator);
    }
  }

  iterator find(const Key &key) {
    const auto index = to_index(key);
    if (!index || !values_[*index].engaged) {
      return end();
    }
    return iterator(this, *index);
  }

  const_iterator find(const Key &key) const {
    const auto index = to_index(key);
    if (!index || !values_[*index].engaged) {
      return end();
    }
    return const_iterator(this, *index);
  }

  iterator begin() {
    auto it = iterator(this, 0);
    it.skip_empty();
    return it;
  }
  const_iterator begin() const {
    auto it = const_iterator(this, 0);
    it.skip_empty();
    return it;
  }
  iterator end() { return iterator(this, Size); }
  const_iterator end() const { return const_iterator(this, Size); }

  Rows &operator[](const Key &key) {
    const auto index = to_index(key);
    if (!index) {
      throw std::out_of_range("dingo array lookup key out of range");
    }
    values_[*index].engaged = true;
    return values_[*index].second;
  }

  iterator erase(iterator it) {
    it->second.clear();
    it->engaged = false;
    ++it;
    return it;
  }

private:
  static std::optional<std::size_t> to_index(const Key &key) {
    static_assert(std::is_integral_v<Key> || std::is_enum_v<Key>,
                  "dingo::array<N> associative backend requires an integral "
                  "or enum key");
    using raw_index_type = typename array_lookup_index_type<Key>::type;
    const auto raw_index = static_cast<raw_index_type>(key);
    if constexpr (std::is_signed_v<raw_index_type>) {
      if (raw_index < 0) {
        return std::nullopt;
      }
    }
    const auto index = static_cast<std::size_t>(raw_index);
    if (index >= Size) {
      return std::nullopt;
    }
    return index;
  }

  storage_type values_;
};

template <typename IndexEntry, bool = std::is_void_v<IndexEntry>>
struct runtime_lookup_entry_cardinality {
  using type = typename IndexEntry::cardinality;
};

template <typename IndexEntry>
struct runtime_lookup_entry_cardinality<IndexEntry, true> {
  using type = ::dingo::one;
};

template <typename KeyDomain> struct is_runtime_key_domain : std::false_type {};

template <typename Key>
struct is_runtime_key_domain<::dingo::runtime_key<Key>> : std::true_type {};

template <typename LookupEntry>
inline constexpr bool is_runtime_lookup_entry_v =
    is_runtime_key_domain<typename LookupEntry::key_domain>::value;

template <typename Entries> struct runtime_lookup_entries;

template <> struct runtime_lookup_entries<type_list<>> {
  using type = type_list<>;
};

template <typename Head, typename... Tail>
struct runtime_lookup_entries<type_list<Head, Tail...>> {
private:
  using head_type = std::conditional_t<is_runtime_lookup_entry_v<Head>,
                                       type_list<Head>, type_list<>>;
  using tail_type = typename runtime_lookup_entries<type_list<Tail...>>::type;

public:
  using type = type_list_cat_t<head_type, tail_type>;
};

template <typename Rtti, typename Allocator, typename Entry,
          typename LookupEntries = type_list<>>
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
    using runtime_conflicts_function = bool (*)(const runtime_lookup_table &,
                                                const Entry &,
                                                const lookup_identity &);
    using runtime_insert_function = bool (*)(runtime_lookup_table &, Entry &,
                                             const lookup_identity &);

    type_index interface_type;
    key_domain domain;
    cardinality lookup_cardinality;
    std::optional<type_index> key_type;
    runtime_lookup_key_owner runtime_key;
    bool enforces_lookup_duplicates;
    runtime_conflicts_function runtime_conflicts = nullptr;
    runtime_insert_function runtime_insert = nullptr;

    lookup_identity(
        type_index resolved_interface_type, key_domain resolved_domain,
        cardinality resolved_cardinality,
        std::optional<type_index> resolved_key_type,
        runtime_lookup_key_owner &&resolved_runtime_key =
            runtime_lookup_key_owner{},
        bool resolved_enforces_lookup_duplicates = true,
        runtime_conflicts_function resolved_runtime_conflicts = nullptr,
        runtime_insert_function resolved_runtime_insert = nullptr)
        : interface_type(std::move(resolved_interface_type)),
          domain(resolved_domain), lookup_cardinality(resolved_cardinality),
          key_type(std::move(resolved_key_type)),
          runtime_key(std::move(resolved_runtime_key)),
          enforces_lookup_duplicates(resolved_enforces_lookup_duplicates),
          runtime_conflicts(resolved_runtime_conflicts),
          runtime_insert(resolved_runtime_insert) {}

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

  template <typename KeyDomain> struct runtime_key_type;

  template <typename Key> struct runtime_key_type<::dingo::runtime_key<Key>> {
    using type = Key;
  };

  struct runtime_bucket_base {
    virtual ~runtime_bucket_base() = default;
    virtual void erase(Entry &entry) = 0;
    virtual bool empty() const = 0;
  };

  template <typename LookupEntry,
            bool IsRuntimeKey = is_runtime_lookup_entry_v<LookupEntry>>
  class runtime_key_storage;

  template <typename LookupEntry>
  class runtime_key_storage<LookupEntry, true> : public runtime_bucket_base {
    using key_type =
        typename runtime_key_type<typename LookupEntry::key_domain>::type;
    using backend_type = typename LookupEntry::backend_type;
    using storage_type =
        typename backend_type::template storage<key_type, row_storage,
                                                Allocator>;

  public:
    explicit runtime_key_storage(Allocator &allocator)
        : allocator_(std::addressof(allocator)), storage_(allocator) {}

    static bool matches(const lookup_identity &identity) {
      return identity.interface_type ==
                 Rtti::template get_type_index<
                     typename LookupEntry::interface_type>() &&
             identity.domain == key_domain::runtime_key &&
             identity.lookup_cardinality ==
                 runtime_lookup_cardinality_value<
                     typename LookupEntry::cardinality>() &&
             identity.key_type &&
             *identity.key_type == Rtti::template get_type_index<key_type>() &&
             identity.runtime_key;
    }

    bool conflicts(const Entry &entry, const lookup_identity &identity) const {
      if (!matches(identity)) {
        return false;
      }
      const auto &key = stored_key(identity);
      auto it = storage_.find(key);
      if (it == storage_.end()) {
        return false;
      }
      for (auto *existing_entry : it->second) {
        for (const auto &existing_identity :
             existing_entry->lookup_identities) {
          if (!matches(existing_identity) ||
              !identity_lookup_value_matches(identity, existing_identity)) {
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

    bool insert(Entry &entry, const lookup_identity &identity) {
      if (!matches(identity)) {
        return false;
      }
      rows_for_key(stored_key(identity)).emplace_back(std::addressof(entry));
      return true;
    }

    void erase(Entry &entry) override {
      auto *entry_ptr = std::addressof(entry);
      for (auto it = storage_.begin(); it != storage_.end();) {
        auto &rows = it->second;
        rows.erase(std::remove(rows.begin(), rows.end(), entry_ptr),
                   rows.end());
        if (rows.empty()) {
          it = storage_.erase(it);
        } else {
          ++it;
        }
      }
    }

    bool empty() const override { return storage_.begin() == storage_.end(); }

    template <typename KeyValue>
    Entry *find_singular(const KeyValue &key_value, bool &ambiguous) const {
      Entry *selected = nullptr;
      std::size_t match_count = 0;
      auto it = storage_.find(key_value);
      if (it != storage_.end()) {
        for (auto *entry : it->second) {
          ++match_count;
          if (match_count == 1) {
            selected = entry;
          }
        }
      }
      ambiguous = match_count > 1;
      return ambiguous ? nullptr : selected;
    }

    template <typename KeyValue, typename Fn>
    std::size_t for_each(const KeyValue &key_value, Fn &&fn) const {
      std::size_t count = 0;
      auto it = storage_.find(key_value);
      if (it != storage_.end()) {
        for (auto *entry : it->second) {
          ++count;
          fn(*entry);
        }
      }
      return count;
    }

    template <typename KeyValue>
    std::size_t count(const KeyValue &key_value) const {
      auto it = storage_.find(key_value);
      return it == storage_.end() ? 0U : it->second.size();
    }

  private:
    template <typename> static constexpr bool dependent_false_v = false;

    template <typename Storage, typename = void>
    struct has_subscript : std::false_type {};

    template <typename Storage>
    struct has_subscript<
        Storage, std::void_t<decltype(std::declval<Storage &>()
                                          [std::declval<const key_type &>()])>>
        : std::true_type {};

    template <typename Storage, typename = void>
    struct has_try_emplace : std::false_type {};

    template <typename Storage>
    struct has_try_emplace<
        Storage,
        std::void_t<decltype(std::declval<Storage &>().try_emplace(
            std::declval<const key_type &>(), std::declval<row_allocator>()))>>
        : std::true_type {};

    row_storage &rows_for_key(const key_type &key) {
      if constexpr (has_try_emplace<storage_type>::value) {
        return storage_
            .try_emplace(
                key, make_lookup_storage_allocator<row_allocator>(*allocator_))
            .first->second;
      } else if constexpr (has_subscript<storage_type>::value) {
        return storage_[key];
      } else {
        static_assert(dependent_false_v<storage_type>,
                      "dingo associative backend storage requires "
                      "operator[](const Key&) or try_emplace(const Key&, "
                      "Rows)");
      }
    }

    static const key_type &stored_key(const lookup_identity &identity) {
      auto *stored = static_cast<runtime_lookup_key_value<key_type> *>(
          identity.runtime_key.get());
      return stored->value;
    }

    Allocator *allocator_;
    storage_type storage_;
  };

  template <typename LookupEntry>
  class runtime_key_storage<LookupEntry, false> : public runtime_bucket_base {
  public:
    explicit runtime_key_storage(Allocator &) {}

    bool matches(const lookup_identity &) const { return false; }
    bool conflicts(const Entry &, const lookup_identity &) const {
      return false;
    }
    bool insert(Entry &, const lookup_identity &) { return false; }
    void erase(Entry &) override {}
    bool empty() const override { return true; }
  };

  struct runtime_bucket_owner {
    runtime_bucket_owner() = default;

    ~runtime_bucket_owner() { reset(); }

    runtime_bucket_owner(const runtime_bucket_owner &) = delete;
    runtime_bucket_owner &operator=(const runtime_bucket_owner &) = delete;

    runtime_bucket_owner(runtime_bucket_owner &&other) noexcept
        : allocator_(other.allocator_), ptr_(other.ptr_),
          destroy_(other.destroy_) {
      other.allocator_ = nullptr;
      other.ptr_ = nullptr;
      other.destroy_ = nullptr;
    }

    runtime_bucket_owner &operator=(runtime_bucket_owner &&other) noexcept {
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

    runtime_bucket_base *get() const { return ptr_; }
    runtime_bucket_base &operator*() const { return *ptr_; }
    runtime_bucket_base *operator->() const { return ptr_; }

    template <typename LookupEntry>
    runtime_key_storage<LookupEntry> &as() const {
      return static_cast<runtime_key_storage<LookupEntry> &>(*ptr_);
    }

    template <typename LookupEntry>
    static runtime_bucket_owner make(Allocator &allocator) {
      using concrete_type = runtime_key_storage<LookupEntry>;
      using concrete_allocator = typename std::conditional_t<
          ::dingo::is_static_allocator_v<Allocator>,
          std::allocator<concrete_type>,
          typename std::allocator_traits<Allocator>::template rebind_alloc<
              concrete_type>>;
      using concrete_allocator_traits =
          std::allocator_traits<concrete_allocator>;

      auto concrete_alloc =
          make_lookup_storage_allocator<concrete_allocator>(allocator);
      auto *ptr = concrete_allocator_traits::allocate(concrete_alloc, 1);
      try {
        concrete_allocator_traits::construct(concrete_alloc, ptr, allocator);
      } catch (...) {
        concrete_allocator_traits::deallocate(concrete_alloc, ptr, 1);
        throw;
      }

      runtime_bucket_owner owner;
      owner.allocator_ = std::addressof(allocator);
      owner.ptr_ = ptr;
      owner.destroy_ = &destroy_bucket<LookupEntry>;
      return owner;
    }

  private:
    template <typename LookupEntry>
    static void destroy_bucket(Allocator &allocator,
                               runtime_bucket_base *ptr) noexcept {
      using concrete_type = runtime_key_storage<LookupEntry>;
      using concrete_allocator = typename std::conditional_t<
          ::dingo::is_static_allocator_v<Allocator>,
          std::allocator<concrete_type>,
          typename std::allocator_traits<Allocator>::template rebind_alloc<
              concrete_type>>;
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
    runtime_bucket_base *ptr_ = nullptr;
    void (*destroy_)(Allocator &, runtime_bucket_base *) noexcept = nullptr;
  };

  using runtime_bucket_map =
      dynamic_identity_map<bucket_key, runtime_bucket_owner, Allocator,
                           bucket_key_compare>;

  template <typename Entries> class runtime_key_storages;

  template <typename... Entries>
  class runtime_key_storages<type_list<Entries...>> {
  public:
    explicit runtime_key_storages(Allocator &allocator)
        : allocator_(std::addressof(allocator)), buckets_(allocator) {}

    void erase(Entry &entry) {
      for (auto it = buckets_.begin(); it != buckets_.end();) {
        it->second->erase(entry);
        if (it->second->empty()) {
          it = buckets_.erase(it);
        } else {
          ++it;
        }
      }
    }

    template <typename LookupEntry, typename KeyValue>
    Entry *find_singular(const KeyValue &key_value, bool &ambiguous) const {
      if (auto *bucket = find_bucket<LookupEntry>()) {
        return bucket->find_singular(key_value, ambiguous);
      }
      ambiguous = false;
      return nullptr;
    }

    template <typename LookupEntry, typename KeyValue, typename Fn>
    std::size_t for_each(const KeyValue &key_value, Fn &&fn) const {
      if (auto *bucket = find_bucket<LookupEntry>()) {
        return bucket->for_each(key_value, std::forward<Fn>(fn));
      }
      return 0;
    }

    template <typename LookupEntry, typename KeyValue>
    std::size_t count(const KeyValue &key_value) const {
      if (auto *bucket = find_bucket<LookupEntry>()) {
        return bucket->count(key_value);
      }
      return 0;
    }

    template <typename LookupEntry>
    bool conflicts(const Entry &entry, const lookup_identity &identity) const {
      if (auto *bucket = find_bucket<LookupEntry>()) {
        return bucket->conflicts(entry, identity);
      }
      return false;
    }

    template <typename LookupEntry>
    bool insert(Entry &entry, const lookup_identity &identity) {
      if (!runtime_key_storage<LookupEntry>::matches(identity)) {
        return false;
      }
      ensure_bucket<LookupEntry>().insert(entry, identity);
      return true;
    }

  private:
    template <typename LookupEntry>
    using lookup_key_type =
        typename runtime_key_type<typename LookupEntry::key_domain>::type;

    template <typename LookupEntry> static bucket_key make_bucket() {
      return make_runtime_key_bucket<typename LookupEntry::interface_type,
                                     lookup_key_type<LookupEntry>,
                                     typename LookupEntry::cardinality>();
    }

    template <typename LookupEntry>
    runtime_key_storage<LookupEntry> *find_bucket() {
      auto *owner = buckets_.get(make_bucket<LookupEntry>());
      return owner ? std::addressof(owner->template as<LookupEntry>())
                   : nullptr;
    }

    template <typename LookupEntry>
    const runtime_key_storage<LookupEntry> *find_bucket() const {
      auto *owner = buckets_.get(make_bucket<LookupEntry>());
      return owner ? std::addressof(owner->template as<LookupEntry>())
                   : nullptr;
    }

    template <typename LookupEntry>
    runtime_key_storage<LookupEntry> &ensure_bucket() {
      auto resolved_bucket = make_bucket<LookupEntry>();
      auto *owner = buckets_.get(resolved_bucket);
      if (!owner) {
        owner = std::addressof(
            buckets_
                .insert(std::move(resolved_bucket),
                        runtime_bucket_owner::template make<LookupEntry>(
                            *allocator_))
                .first);
      }
      return owner->template as<LookupEntry>();
    }

    Allocator *allocator_;
    runtime_bucket_map buckets_;
  };

  explicit runtime_lookup_table(Allocator &allocator)
      : allocator_(std::addressof(allocator)), buckets_(allocator),
        runtime_key_buckets_(allocator) {}

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

  template <typename LookupEntry>
  static bool runtime_identity_conflicts(const runtime_lookup_table &table,
                                         const Entry &entry,
                                         const lookup_identity &identity) {
    return table.runtime_key_buckets_.template conflicts<LookupEntry>(entry,
                                                                      identity);
  }

  template <typename LookupEntry>
  static bool runtime_identity_insert(runtime_lookup_table &table, Entry &entry,
                                      const lookup_identity &identity) {
    return table.runtime_key_buckets_.template insert<LookupEntry>(entry,
                                                                   identity);
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename LookupEntry, typename KeyValue>
  static lookup_identity make_runtime_key_identity(Allocator &allocator,
                                                   KeyValue &&key_value) {
    using stored_key_type = std::decay_t<Key>;
    auto erased_key = runtime_lookup_key_owner::template make<stored_key_type>(
        allocator, std::forward<KeyValue>(key_value));
    return lookup_identity{Rtti::template get_type_index<Interface>(),
                           key_domain::runtime_key,
                           runtime_lookup_cardinality_value<Cardinality>(),
                           Rtti::template get_type_index<stored_key_type>(),
                           std::move(erased_key),
                           true,
                           &runtime_identity_conflicts<LookupEntry>,
                           &runtime_identity_insert<LookupEntry>};
  }

  bool insert(Entry &entry) {
    for (const auto &identity : entry.lookup_identities) {
      if (conflicts(entry, identity)) {
        return false;
      }
    }

    try {
      for (const auto &identity : entry.lookup_identities) {
        if (identity.domain == key_domain::runtime_key) {
          if (!identity.runtime_insert ||
              !identity.runtime_insert(*this, entry, identity)) {
            return false;
          }
        } else {
          auto &rows = bucket(identity);
          rows.emplace_back(std::addressof(entry));
        }
      }
    } catch (...) {
      erase(entry);
      throw;
    }

    return true;
  }

  void erase(Entry &entry) {
    runtime_key_buckets_.erase(entry);
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
            typename LookupEntry, typename KeyValue>
  Entry *find_singular_runtime_key(const KeyValue &key_value,
                                   bool &ambiguous) const {
    return runtime_key_buckets_.template find_singular<LookupEntry>(key_value,
                                                                    ambiguous);
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
            typename LookupEntry, typename KeyValue, typename Fn>
  std::size_t for_each_runtime_key(const KeyValue &key_value, Fn &&fn) const {
    return runtime_key_buckets_.template for_each<LookupEntry>(
        key_value, std::forward<Fn>(fn));
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
            typename LookupEntry, typename KeyValue>
  std::size_t count_runtime_key(const KeyValue &key_value) const {
    return runtime_key_buckets_.template count<LookupEntry>(key_value);
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
    if (identity.domain == key_domain::runtime_key) {
      return identity.runtime_conflicts &&
             identity.runtime_conflicts(*this, entry, identity);
    }
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
  runtime_key_storages<typename runtime_lookup_entries<LookupEntries>::type>
      runtime_key_buckets_;
};

} // namespace dingo::detail
