//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/memory/aligned_storage.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/type/dynamic_identity_map.h>
#include <dingo/view/view.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
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

template <typename LookupEntry, bool = std::is_void_v<LookupEntry>>
struct runtime_lookup_entry_cardinality {
  using type = typename LookupEntry::cardinality;
};

template <typename LookupEntry>
struct runtime_lookup_entry_cardinality<LookupEntry, true> {
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

template <typename T, std::size_t InlineCapacity, typename InlineAllocator>
class runtime_lookup_small_vector {
  static_assert(InlineCapacity > 0);

public:
  using allocator_type = InlineAllocator;

  explicit runtime_lookup_small_vector(
      const allocator_type &allocator = allocator_type())
      : dynamic_(allocator) {}

  void emplace_back(T value) {
    if (inline_size_ < InlineCapacity) {
      inline_[inline_size_++] = std::move(value);
      return;
    }
    dynamic_.emplace_back(std::move(value));
  }

  bool empty() const { return size() == 0; }

  std::size_t size() const { return inline_size_ + dynamic_.size(); }

  T front() const { return inline_.front(); }

  void clear() {
    inline_size_ = 0;
    dynamic_.clear();
  }

  void erase(const T &value) {
    for (std::size_t i = 0; i < inline_size_; ++i) {
      if (inline_[i] == value) {
        erase_inline(i);
        return;
      }
    }

    auto it = std::find(dynamic_.begin(), dynamic_.end(), value);
    if (it != dynamic_.end()) {
      dynamic_.erase(it);
    }
  }

  template <typename Fn> void for_each(Fn &&fn) const {
    for (std::size_t i = 0; i < inline_size_; ++i) {
      fn(inline_[i]);
    }
    for (const auto &value : dynamic_) {
      fn(value);
    }
  }

private:
  void erase_inline(std::size_t offset) {
    for (std::size_t i = offset + 1; i < inline_size_; ++i) {
      inline_[i - 1] = std::move(inline_[i]);
    }

    if (!dynamic_.empty()) {
      inline_[inline_size_ - 1] = std::move(dynamic_.front());
      dynamic_.erase(dynamic_.begin());
      return;
    }

    --inline_size_;
  }

  std::array<T, InlineCapacity> inline_{};
  std::size_t inline_size_ = 0;
  std::vector<T, allocator_type> dynamic_;
};

template <typename Entry, typename RowAllocator,
          runtime_lookup_cardinality Cardinality>
class row_storage;

template <typename Entry, typename RowAllocator>
class row_storage<Entry, RowAllocator, runtime_lookup_cardinality::one> {
public:
  using allocator_type = RowAllocator;

  explicit row_storage(const RowAllocator & = RowAllocator()) {}

  void emplace_back(Entry *entry) { entry_ = entry; }

  bool empty() const { return entry_ == nullptr; }

  std::size_t size() const { return entry_ ? 1U : 0U; }

  Entry *front() const { return entry_; }

  void clear() { entry_ = nullptr; }

  void erase(Entry *entry) {
    if (entry_ == entry) {
      entry_ = nullptr;
    }
  }

  template <typename Fn> void for_each(Fn &&fn) const {
    if (entry_) {
      fn(entry_);
    }
  }

private:
  Entry *entry_ = nullptr;
};

template <typename Entry, typename RowAllocator>
class row_storage<Entry, RowAllocator, runtime_lookup_cardinality::many> {
public:
  using allocator_type = RowAllocator;

  explicit row_storage(const RowAllocator &allocator = RowAllocator())
      : entries_(allocator) {}

  void emplace_back(Entry *entry) { entries_.emplace_back(entry); }

  bool empty() const { return entries_.empty(); }

  std::size_t size() const { return entries_.size(); }

  Entry *front() const { return entries_.front(); }

  void clear() { entries_.clear(); }

  void erase(Entry *entry) { entries_.erase(entry); }

  template <typename Fn> void for_each(Fn &&fn) const {
    entries_.for_each(std::forward<Fn>(fn));
  }

private:
  runtime_lookup_small_vector<Entry *, 1, RowAllocator> entries_;
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

    runtime_lookup_key_owner(runtime_lookup_key_owner &&other) noexcept {
      move_from(other);
    }

    runtime_lookup_key_owner &
    operator=(runtime_lookup_key_owner &&other) noexcept {
      if (this != &other) {
        reset();
        move_from(other);
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
      using concrete_type = runtime_lookup_key_value<stored_key_type>;

      if constexpr (can_store_inline<concrete_type>) {
        runtime_lookup_key_owner owner;
        auto *ptr = reinterpret_cast<concrete_type *>(
            std::addressof(owner.inline_storage_));
        new (ptr) concrete_type(std::forward<KeyValue>(key_value));
        owner.allocator_ = std::addressof(allocator);
        owner.ptr_ = ptr;
        owner.destroy_ = &destroy_inline_key<stored_key_type>;
        owner.move_ = &move_inline_key<stored_key_type>;
        return owner;
      } else {
        return make_allocated<stored_key_type>(
            allocator, std::forward<KeyValue>(key_value));
      }
    }

  private:
    static constexpr std::size_t inline_key_storage_size = 64;
    static constexpr std::size_t inline_key_storage_alignment =
        alignof(std::max_align_t);

    template <typename Concrete>
    static constexpr bool can_store_inline =
        sizeof(Concrete) <= inline_key_storage_size &&
        alignof(Concrete) <= inline_key_storage_alignment &&
        std::is_nothrow_move_constructible_v<Concrete>;

    template <typename Key, typename KeyValue>
    static runtime_lookup_key_owner make_allocated(Allocator &allocator,
                                                   KeyValue &&key_value) {
      using concrete_allocator = runtime_lookup_key_allocator<Key>;
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
      owner.destroy_ = &destroy_key<Key>;
      return owner;
    }

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

    template <typename Key>
    static void destroy_inline_key(Allocator &,
                                   erased_runtime_lookup_key *ptr) noexcept {
      using concrete_type = runtime_lookup_key_value<Key>;
      static_cast<concrete_type *>(ptr)->~concrete_type();
    }

    template <typename Key>
    static void move_inline_key(runtime_lookup_key_owner &destination,
                                runtime_lookup_key_owner &source) noexcept {
      using concrete_type = runtime_lookup_key_value<Key>;
      auto *source_ptr = static_cast<concrete_type *>(source.ptr_);
      auto *destination_ptr = reinterpret_cast<concrete_type *>(
          std::addressof(destination.inline_storage_));
      new (destination_ptr) concrete_type(std::move(*source_ptr));
      source_ptr->~concrete_type();

      destination.allocator_ = source.allocator_;
      destination.ptr_ = destination_ptr;
      destination.destroy_ = source.destroy_;
      destination.move_ = source.move_;
      source.clear();
    }

    void move_from(runtime_lookup_key_owner &other) noexcept {
      if (!other.ptr_) {
        return;
      }
      if (other.move_) {
        other.move_(*this, other);
        return;
      }

      allocator_ = other.allocator_;
      ptr_ = other.ptr_;
      destroy_ = other.destroy_;
      other.clear();
    }

    void clear() noexcept {
      allocator_ = nullptr;
      ptr_ = nullptr;
      destroy_ = nullptr;
      move_ = nullptr;
    }

    void reset() noexcept {
      if (ptr_) {
        destroy_(*allocator_, ptr_);
        clear();
      }
    }

    dingo::aligned_storage_t<inline_key_storage_size,
                             inline_key_storage_alignment>
        inline_storage_;
    Allocator *allocator_ = nullptr;
    erased_runtime_lookup_key *ptr_ = nullptr;
    void (*destroy_)(Allocator &,
                     erased_runtime_lookup_key *) noexcept = nullptr;
    void (*move_)(runtime_lookup_key_owner &,
                  runtime_lookup_key_owner &) noexcept = nullptr;
  };

  struct lookup_identity {
    type_index interface_type;
    key_domain domain;
    cardinality lookup_cardinality;
    std::optional<type_index> key_type;
    runtime_lookup_key_owner runtime_key;

    lookup_identity(type_index resolved_interface_type,
                    key_domain resolved_domain,
                    cardinality resolved_cardinality,
                    std::optional<type_index> resolved_key_type,
                    runtime_lookup_key_owner &&resolved_runtime_key =
                        runtime_lookup_key_owner{})
        : interface_type(std::move(resolved_interface_type)),
          domain(resolved_domain), lookup_cardinality(resolved_cardinality),
          key_type(std::move(resolved_key_type)),
          runtime_key(std::move(resolved_runtime_key)) {}

    lookup_identity(const lookup_identity &) = delete;
    lookup_identity &operator=(const lookup_identity &) = delete;
    lookup_identity(lookup_identity &&) noexcept = default;
    lookup_identity &operator=(lookup_identity &&) noexcept = default;
  };

  using row_allocator = typename std::conditional_t<
      ::dingo::is_static_allocator_v<Allocator>, std::allocator<Entry *>,
      typename std::allocator_traits<Allocator>::template rebind_alloc<
          Entry *>>;

  template <runtime_lookup_cardinality Cardinality>
  using row_storage_type = row_storage<Entry, row_allocator, Cardinality>;

  struct bucket_key {
    key_domain domain;
    cardinality lookup_cardinality;
    std::optional<type_index> key_type;
  };

  struct bucket_key_compare {
    bool operator()(const bucket_key &lhs, const bucket_key &rhs) const {
      std::less<type_index> less;
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

  template <runtime_lookup_cardinality Cardinality>
  using bucket_map =
      dynamic_identity_map<bucket_key, row_storage_type<Cardinality>, Allocator,
                           bucket_key_compare>;

  template <runtime_lookup_cardinality Cardinality> struct ordinary_bucket {
    ordinary_bucket(bucket_key resolved_key, const row_allocator &row_alloc)
        : key(std::move(resolved_key)), rows(row_alloc) {}

    bucket_key key;
    row_storage_type<Cardinality> rows;
  };

  struct runtime_bucket_key {
    type_index interface_type;
    key_domain domain;
    cardinality lookup_cardinality;
    std::optional<type_index> key_type;
  };

  struct runtime_bucket_key_compare {
    bool operator()(const runtime_bucket_key &lhs,
                    const runtime_bucket_key &rhs) const {
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

  template <typename KeyDomain> struct runtime_key_type;

  template <typename Key> struct runtime_key_type<::dingo::runtime_key<Key>> {
    using type = Key;
  };

  template <runtime_lookup_cardinality Cardinality>
  static void erase_row(row_storage_type<Cardinality> &rows, Entry *entry) {
    rows.erase(entry);
  }

  template <runtime_lookup_cardinality Cardinality>
  static bool row_conflicts(const row_storage_type<Cardinality> &rows,
                            const Entry &entry) {
    if constexpr (Cardinality == cardinality::one) {
      (void)entry;
      return !rows.empty();
    }

    bool result = false;
    rows.for_each([&](auto *existing_entry) {
      if (existing_entry->storage_type == entry.storage_type) {
        result = true;
      }
    });
    return result;
  }

  template <runtime_lookup_cardinality Cardinality>
  static Entry *singular_row(const row_storage_type<Cardinality> *rows,
                             bool &ambiguous) {
    if (!rows || rows->empty()) {
      ambiguous = false;
      return nullptr;
    }
    ambiguous = rows->size() > 1;
    return ambiguous ? nullptr : rows->front();
  }

  template <runtime_lookup_cardinality Cardinality, typename Fn>
  static std::size_t for_each_row(const row_storage_type<Cardinality> *rows,
                                  Fn &&fn) {
    if (!rows) {
      return 0;
    }
    rows->for_each([&](auto *entry) { fn(*entry); });
    return rows->size();
  }

  struct runtime_bucket_base {
    virtual ~runtime_bucket_base() = default;
    virtual void erase(Entry &entry, const lookup_identity &identity) = 0;
  };

  template <typename LookupEntry>
  class runtime_key_storage : public runtime_bucket_base {
    using key_type =
        typename runtime_key_type<typename LookupEntry::key_domain>::type;
    using backend_type = typename LookupEntry::backend_type;
    static constexpr auto lookup_cardinality =
        runtime_lookup_cardinality_value<typename LookupEntry::cardinality>();
    using runtime_row_storage = row_storage_type<lookup_cardinality>;
    using storage_type =
        typename backend_type::template storage<key_type, runtime_row_storage,
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
      return row_conflicts<lookup_cardinality>(it->second, entry);
    }

    bool insert(Entry &entry, const lookup_identity &identity) {
      if (!matches(identity)) {
        return false;
      }
      rows_for_key(stored_key(identity)).emplace_back(std::addressof(entry));
      return true;
    }

    void erase(Entry &entry, const lookup_identity &identity) override {
      if (!matches(identity)) {
        return;
      }
      auto *entry_ptr = std::addressof(entry);
      auto it = storage_.find(stored_key(identity));
      if (it == storage_.end()) {
        return;
      }
      auto &rows = it->second;
      erase_row<lookup_cardinality>(rows, entry_ptr);
      if (rows.empty()) {
        storage_.erase(it);
      }
    }

    template <typename KeyValue>
    Entry *find_singular(const KeyValue &key_value, bool &ambiguous) const {
      auto it = storage_.find(key_value);
      return singular_row<lookup_cardinality>(
          it == storage_.end() ? nullptr : std::addressof(it->second),
          ambiguous);
    }

    template <typename KeyValue, typename Fn>
    std::size_t for_each(const KeyValue &key_value, Fn &&fn) const {
      auto it = storage_.find(key_value);
      return for_each_row<lookup_cardinality>(
          it == storage_.end() ? nullptr : std::addressof(it->second),
          std::forward<Fn>(fn));
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

    runtime_row_storage &rows_for_key(const key_type &key) {
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
      dynamic_identity_map<runtime_bucket_key, runtime_bucket_owner, Allocator,
                           runtime_bucket_key_compare>;

  template <typename Entries> class runtime_key_storages;

  template <typename... Entries>
  class runtime_key_storages<type_list<Entries...>> {
  public:
    explicit runtime_key_storages(Allocator &allocator)
        : allocator_(std::addressof(allocator)), buckets_(allocator) {}

    void erase(Entry &entry) {
      const auto &identity = entry.identity;
      if (identity.domain != key_domain::runtime_key) {
        return;
      }
      auto *owner =
          buckets_.get(runtime_lookup_table::make_runtime_bucket(identity));
      if (owner) {
        (*owner)->erase(entry, identity);
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

    bool conflicts(const Entry &entry, const lookup_identity &identity) const {
      bool result = false;
      (void)std::initializer_list<int>{
          ((result = result || conflicts_one<Entries>(entry, identity)), 0)...};
      return result;
    }

    bool insert(Entry &entry, const lookup_identity &identity) {
      bool inserted = false;
      (void)std::initializer_list<int>{(
          (inserted = inserted || insert_one<Entries>(entry, identity)), 0)...};
      return inserted;
    }

  private:
    template <typename LookupEntry>
    using lookup_key_type =
        typename runtime_key_type<typename LookupEntry::key_domain>::type;

    template <typename LookupEntry> static runtime_bucket_key make_bucket() {
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

    template <typename LookupEntry>
    bool conflicts_one(const Entry &entry,
                       const lookup_identity &identity) const {
      if (auto *bucket = find_bucket<LookupEntry>()) {
        return bucket->conflicts(entry, identity);
      }
      return false;
    }

    template <typename LookupEntry>
    bool insert_one(Entry &entry, const lookup_identity &identity) {
      if (!runtime_key_storage<LookupEntry>::matches(identity)) {
        return false;
      }
      ensure_bucket<LookupEntry>().insert(entry, identity);
      return true;
    }

    Allocator *allocator_;
    runtime_bucket_map buckets_;
  };

  explicit runtime_lookup_table(Allocator &allocator)
      : allocator_(std::addressof(allocator)), one_buckets_(allocator),
        many_buckets_(allocator), runtime_key_buckets_(allocator) {}

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
    const auto &identity = entry.identity;
    if (conflicts(entry, identity)) {
      return false;
    }

    try {
      if (identity.domain == key_domain::runtime_key) {
        if (!runtime_key_buckets_.insert(entry, identity)) {
          return false;
        }
      } else if (identity.lookup_cardinality == cardinality::one) {
        insert_ordinary<cardinality::one>(entry, identity);
      } else {
        insert_ordinary<cardinality::many>(entry, identity);
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
    const auto &identity = entry.identity;
    if (identity.domain == key_domain::runtime_key) {
      return;
    }
    if (identity.lookup_cardinality == cardinality::one) {
      erase_ordinary<cardinality::one>(identity, entry_ptr);
    } else {
      erase_ordinary<cardinality::many>(identity, entry_ptr);
    }
  }

  template <typename Interface, typename Cardinality>
  Entry *find_singular_no_key(bool &ambiguous) const {
    return find_singular_in_bucket<
        runtime_lookup_cardinality_value<Cardinality>()>(
        make_no_key_bucket<Interface, Cardinality>(), ambiguous);
  }

  template <typename Interface, typename Key, typename Cardinality>
  Entry *find_singular_typed_key(bool &ambiguous) const {
    return find_singular_in_bucket<
        runtime_lookup_cardinality_value<Cardinality>()>(
        make_typed_key_bucket<Interface, Key, Cardinality>(), ambiguous);
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
    return for_each_in_bucket<runtime_lookup_cardinality_value<Cardinality>()>(
        make_no_key_bucket<Interface, Cardinality>(), std::forward<Fn>(fn));
  }

  template <typename Interface, typename Key, typename Cardinality, typename Fn>
  std::size_t for_each_typed_key(Fn &&fn) const {
    return for_each_in_bucket<runtime_lookup_cardinality_value<Cardinality>()>(
        make_typed_key_bucket<Interface, Key, Cardinality>(),
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
    return count_in_bucket<runtime_lookup_cardinality_value<Cardinality>()>(
        make_no_key_bucket<Interface, Cardinality>());
  }

  template <typename Interface, typename Key, typename Cardinality>
  std::size_t count_typed_key() const {
    return count_in_bucket<runtime_lookup_cardinality_value<Cardinality>()>(
        make_typed_key_bucket<Interface, Key, Cardinality>());
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename LookupEntry, typename KeyValue>
  std::size_t count_runtime_key(const KeyValue &key_value) const {
    return runtime_key_buckets_.template count<LookupEntry>(key_value);
  }

private:
  static bucket_key make_bucket(const lookup_identity &identity) {
    return {identity.domain, identity.lookup_cardinality, identity.key_type};
  }

  static runtime_bucket_key
  make_runtime_bucket(const lookup_identity &identity) {
    return {identity.interface_type, identity.domain,
            identity.lookup_cardinality, identity.key_type};
  }

  template <typename Interface, typename Cardinality>
  static bucket_key make_no_key_bucket() {
    return {key_domain::no_key, runtime_lookup_cardinality_value<Cardinality>(),
            std::nullopt};
  }

  template <typename Interface, typename Key, typename Cardinality>
  static bucket_key make_typed_key_bucket() {
    return {key_domain::typed_key,
            runtime_lookup_cardinality_value<Cardinality>(),
            Rtti::template get_type_index<Key>()};
  }

  template <typename Interface, typename Key, typename Cardinality>
  static runtime_bucket_key make_runtime_key_bucket() {
    using stored_key_type = std::decay_t<Key>;
    return {Rtti::template get_type_index<Interface>(), key_domain::runtime_key,
            runtime_lookup_cardinality_value<Cardinality>(),
            Rtti::template get_type_index<stored_key_type>()};
  }

  template <cardinality Cardinality>
  void insert_ordinary(Entry &entry, const lookup_identity &identity) {
    auto &rows = bucket<Cardinality>(identity);
    rows.emplace_back(std::addressof(entry));
  }

  template <cardinality Cardinality>
  void erase_ordinary(const lookup_identity &identity, Entry *entry_ptr) {
    auto resolved_bucket = make_bucket(identity);
    if (auto *rows = inline_bucket_rows<Cardinality>(resolved_bucket)) {
      erase_row<Cardinality>(*rows, entry_ptr);
      if (rows->empty()) {
        inline_bucket<Cardinality>().reset();
      }
      return;
    }
    auto *rows = buckets<Cardinality>().get(resolved_bucket);
    if (rows) {
      erase_row<Cardinality>(*rows, entry_ptr);
      if (rows->empty()) {
        buckets<Cardinality>().erase(resolved_bucket);
      }
    }
  }

  template <cardinality Cardinality>
  row_storage_type<Cardinality> &bucket(const lookup_identity &identity) {
    auto resolved_bucket = make_bucket(identity);
    if (auto *rows = inline_bucket_rows<Cardinality>(resolved_bucket)) {
      return *rows;
    }
    if (auto *rows = buckets<Cardinality>().get(resolved_bucket)) {
      return *rows;
    }
    if (!inline_bucket<Cardinality>()) {
      inline_bucket<Cardinality>().emplace(
          std::move(resolved_bucket),
          make_lookup_storage_allocator<row_allocator>(*allocator_));
      return inline_bucket<Cardinality>()->rows;
    }
    return buckets<Cardinality>()
        .insert(std::move(resolved_bucket),
                make_lookup_storage_allocator<row_allocator>(*allocator_))
        .first;
  }

  template <cardinality Cardinality>
  Entry *find_singular_in_bucket(const bucket_key &resolved_bucket,
                                 bool &ambiguous) const {
    if (const auto *rows = inline_bucket_rows<Cardinality>(resolved_bucket)) {
      return singular_row<Cardinality>(rows, ambiguous);
    }
    return singular_row<Cardinality>(
        buckets<Cardinality>().get(resolved_bucket), ambiguous);
  }

  template <cardinality Cardinality, typename Fn>
  std::size_t for_each_in_bucket(const bucket_key &resolved_bucket,
                                 Fn &&fn) const {
    if (const auto *rows = inline_bucket_rows<Cardinality>(resolved_bucket)) {
      return for_each_row<Cardinality>(rows, std::forward<Fn>(fn));
    }
    return for_each_row<Cardinality>(
        buckets<Cardinality>().get(resolved_bucket), std::forward<Fn>(fn));
  }

  template <cardinality Cardinality>
  std::size_t count_in_bucket(const bucket_key &resolved_bucket) const {
    if (const auto *rows = inline_bucket_rows<Cardinality>(resolved_bucket)) {
      return rows->size();
    }
    if (const auto *rows = buckets<Cardinality>().get(resolved_bucket)) {
      return rows->size();
    }
    return 0;
  }

  bool conflicts(const Entry &entry, const lookup_identity &identity) const {
    if (identity.domain == key_domain::runtime_key) {
      return runtime_key_buckets_.conflicts(entry, identity);
    }
    if (identity.lookup_cardinality == cardinality::one) {
      return conflicts_ordinary<cardinality::one>(entry, identity);
    }
    return conflicts_ordinary<cardinality::many>(entry, identity);
  }

  template <cardinality Cardinality>
  bool conflicts_ordinary(const Entry &entry,
                          const lookup_identity &identity) const {
    auto resolved_bucket = make_bucket(identity);
    auto *rows = inline_bucket_rows<Cardinality>(resolved_bucket);
    if (!rows) {
      rows = buckets<Cardinality>().get(resolved_bucket);
    }
    if (!rows) {
      return false;
    }
    return row_conflicts<Cardinality>(*rows, entry);
  }

  static bool bucket_matches(const bucket_key &lhs, const bucket_key &rhs) {
    return lhs.domain == rhs.domain &&
           lhs.lookup_cardinality == rhs.lookup_cardinality &&
           lhs.key_type == rhs.key_type;
  }

  template <cardinality Cardinality>
  std::optional<ordinary_bucket<Cardinality>> &inline_bucket() {
    if constexpr (Cardinality == cardinality::one) {
      return inline_one_bucket_;
    } else {
      return inline_many_bucket_;
    }
  }

  template <cardinality Cardinality>
  const std::optional<ordinary_bucket<Cardinality>> &inline_bucket() const {
    if constexpr (Cardinality == cardinality::one) {
      return inline_one_bucket_;
    } else {
      return inline_many_bucket_;
    }
  }

  template <cardinality Cardinality> bucket_map<Cardinality> &buckets() {
    if constexpr (Cardinality == cardinality::one) {
      return one_buckets_;
    } else {
      return many_buckets_;
    }
  }

  template <cardinality Cardinality>
  const bucket_map<Cardinality> &buckets() const {
    if constexpr (Cardinality == cardinality::one) {
      return one_buckets_;
    } else {
      return many_buckets_;
    }
  }

  template <cardinality Cardinality>
  row_storage_type<Cardinality> *
  inline_bucket_rows(const bucket_key &resolved_bucket) {
    return inline_bucket<Cardinality>() &&
                   bucket_matches(inline_bucket<Cardinality>()->key,
                                  resolved_bucket)
               ? std::addressof(inline_bucket<Cardinality>()->rows)
               : nullptr;
  }

  template <cardinality Cardinality>
  const row_storage_type<Cardinality> *
  inline_bucket_rows(const bucket_key &resolved_bucket) const {
    return inline_bucket<Cardinality>() &&
                   bucket_matches(inline_bucket<Cardinality>()->key,
                                  resolved_bucket)
               ? std::addressof(inline_bucket<Cardinality>()->rows)
               : nullptr;
  }

  Allocator *allocator_;
  std::optional<ordinary_bucket<cardinality::one>> inline_one_bucket_;
  std::optional<ordinary_bucket<cardinality::many>> inline_many_bucket_;
  bucket_map<cardinality::one> one_buckets_;
  bucket_map<cardinality::many> many_buckets_;
  runtime_key_storages<typename runtime_lookup_entries<LookupEntries>::type>
      runtime_key_buckets_;
};

} // namespace dingo::detail
