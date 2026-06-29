//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/exceptions.h>
#include <dingo/memory/allocator.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <iterator>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace dingo {
template <typename... Projections> struct selectors {};
struct no_key {};
template <typename Key> struct typed_key {};
template <typename Key> struct runtime_key {};
struct one {};
struct many {};
template <typename Interface, typename KeyDomain, typename Cardinality>
struct selector {};
template <typename Interface>
using collection = selector<Interface, no_key, many>;
template <typename Interface, typename Key, typename Cardinality = one>
using associative = selector<Interface, runtime_key<Key>, Cardinality>;
template <typename Interface, typename Key>
using unique_index = selector<Interface, runtime_key<Key>, one>;
template <typename Interface, typename Key>
using multi_index = selector<Interface, runtime_key<Key>, many>;
template <typename... Args> struct interfaces;
template <typename T, auto... Values> struct key;

namespace detail {
template <typename...> inline constexpr bool index_dependent_false_v = false;

template <typename Interface, typename KeyDomain, typename Cardinality>
struct index_entry {
  using interface_type = Interface;
  using key_domain = KeyDomain;
  using cardinality = Cardinality;
};

template <typename T> struct index_interface_arg {
  using type = type_list<T>;
};

template <typename... Types> struct index_interface_arg<interfaces<Types...>> {
  using type = type_list<Types...>;
};

template <typename T> struct index_key_arg {
  using type = T;
};

template <typename T, auto... Values> struct index_key_arg<key<T, Values...>> {
  static_assert(sizeof...(Values) == 0,
                "index definitions require dingo::key<Key>, not "
                "dingo::key<Key, Value>");
  using type = T;
};

template <typename T> using normalized_index_interface_t = normalized_type_t<T>;

template <typename T>
using normalized_index_key_t =
    std::remove_cv_t<std::remove_reference_t<typename index_key_arg<T>::type>>;

template <typename KeyDomain> struct index_key_domain_type;

template <typename Key>
struct index_key_domain_type<::dingo::runtime_key<Key>> {
  using type = Key;
};

template <typename Key> struct index_key_domain_type<::dingo::typed_key<Key>> {
  using type = Key;
};

template <> struct index_key_domain_type<::dingo::no_key> {
  using type = void;
};

template <typename KeyDomain>
using index_key_domain_type_t = typename index_key_domain_type<KeyDomain>::type;

template <typename Interfaces, typename KeyDomain, typename Cardinality>
struct normalize_index_interfaces;

template <typename... Interfaces, typename KeyDomain, typename Cardinality>
struct normalize_index_interfaces<type_list<Interfaces...>, KeyDomain,
                                  Cardinality> {
  using type = type_list<index_entry<normalized_index_interface_t<Interfaces>,
                                     KeyDomain, Cardinality>...>;
};

template <typename Definition> struct normalize_index_definition;

template <typename Interface, typename KeyDomain, typename Cardinality>
struct normalize_index_definition<
    ::dingo::selector<Interface, KeyDomain, Cardinality>>
    : normalize_index_interfaces<typename index_interface_arg<Interface>::type,
                                 KeyDomain, Cardinality> {};

template <typename DefinitionList> struct normalize_index_definitions;

template <typename... Definitions>
struct normalize_index_definitions<type_list<Definitions...>> {
  using type = type_list_cat_t<
      typename normalize_index_definition<Definitions>::type...>;
};

template <> struct normalize_index_definitions<std::tuple<>> {
  using type = type_list<>;
};

template <typename... Definitions>
struct normalize_index_definitions<::dingo::selectors<Definitions...>>
    : normalize_index_definitions<type_list<Definitions...>> {};

template <typename Definitions>
using normalize_index_definitions_t =
    typename normalize_index_definitions<Definitions>::type;

template <typename Candidate, typename Entry>
struct same_index_slot
    : std::bool_constant<std::is_same_v<typename Candidate::interface_type,
                                        typename Entry::interface_type> &&
                         std::is_same_v<typename Candidate::key_domain,
                                        typename Entry::key_domain>> {};

template <typename Candidate, typename Entry>
struct same_index_projection
    : std::bool_constant<same_index_slot<Candidate, Entry>::value &&
                         std::is_same_v<typename Candidate::cardinality,
                                        typename Entry::cardinality>> {};

template <typename Candidate, typename List> struct contains_index_slot;

template <typename Candidate>
struct contains_index_slot<Candidate, type_list<>> : std::false_type {};

template <typename Candidate, typename Head, typename... Tail>
struct contains_index_slot<Candidate, type_list<Head, Tail...>>
    : std::bool_constant<
          same_index_slot<Candidate, Head>::value ||
          contains_index_slot<Candidate, type_list<Tail...>>::value> {};

template <typename Candidate, typename List> struct contains_index_projection;

template <typename Candidate>
struct contains_index_projection<Candidate, type_list<>> : std::false_type {};

template <typename Candidate, typename Head, typename... Tail>
struct contains_index_projection<Candidate, type_list<Head, Tail...>>
    : std::bool_constant<
          same_index_projection<Candidate, Head>::value ||
          contains_index_projection<Candidate, type_list<Tail...>>::value> {};

template <typename Seen, typename Remaining> struct has_duplicate_index_slot;

template <typename... Seen>
struct has_duplicate_index_slot<type_list<Seen...>, type_list<>>
    : std::false_type {};

template <typename... Seen, typename Head, typename... Tail>
struct has_duplicate_index_slot<type_list<Seen...>, type_list<Head, Tail...>>
    : std::bool_constant<contains_index_slot<Head, type_list<Seen...>>::value ||
                         has_duplicate_index_slot<type_list<Seen..., Head>,
                                                  type_list<Tail...>>::value> {
};

template <typename Entries>
inline constexpr bool has_duplicate_index_slot_v =
    has_duplicate_index_slot<type_list<>, Entries>::value;

template <typename Seen, typename Remaining>
struct has_duplicate_index_projection;

template <typename... Seen>
struct has_duplicate_index_projection<type_list<Seen...>, type_list<>>
    : std::false_type {};

template <typename... Seen, typename Head, typename... Tail>
struct has_duplicate_index_projection<type_list<Seen...>,
                                      type_list<Head, Tail...>>
    : std::bool_constant<
          contains_index_projection<Head, type_list<Seen...>>::value ||
          has_duplicate_index_projection<type_list<Seen..., Head>,
                                         type_list<Tail...>>::value> {};

template <typename Entries>
inline constexpr bool has_duplicate_index_projection_v =
    has_duplicate_index_projection<type_list<>, Entries>::value;

template <typename Interface, typename Key, typename Entries>
struct index_entry_for;

template <typename Interface, typename Key>
struct index_entry_for<Interface, Key, type_list<>> {
  using type = void;
};

template <typename Interface, typename Key, typename Head, typename... Tail>
struct index_entry_for<Interface, Key, type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::runtime_key<Key>>;

public:
  using type = std::conditional_t<
      exact, Head,
      typename index_entry_for<Interface, Key, type_list<Tail...>>::type>;
};

template <typename Interface, typename Key, typename Entries>
struct selected_index_entry {
private:
  using normalized_interface = normalized_index_interface_t<Interface>;
  using normalized_key = normalized_index_key_t<Key>;

public:
  using type = typename index_entry_for<normalized_interface, normalized_key,
                                        Entries>::type;
};

template <typename Interface, typename Key, typename Entries>
using selected_index_entry_t =
    typename selected_index_entry<Interface, Key, Entries>::type;

template <typename Interface, typename Cardinality, typename Entries>
struct no_key_index_entry_for;

template <typename Interface, typename Cardinality>
struct no_key_index_entry_for<Interface, Cardinality, type_list<>> {
  using type = void;
};

template <typename Interface, typename Cardinality, typename Head,
          typename... Tail>
struct no_key_index_entry_for<Interface, Cardinality,
                              type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::no_key> &&
      std::is_same_v<typename Head::cardinality, Cardinality>;

public:
  using type =
      std::conditional_t<exact, Head,
                         typename no_key_index_entry_for<
                             Interface, Cardinality, type_list<Tail...>>::type>;
};

template <typename Interface, typename Cardinality, typename Entries>
struct selected_no_key_index_entry {
private:
  using normalized_interface = normalized_index_interface_t<Interface>;

public:
  using type = typename no_key_index_entry_for<normalized_interface,
                                               Cardinality, Entries>::type;
};

template <typename Interface, typename Cardinality, typename Entries>
using selected_no_key_index_entry_t =
    typename selected_no_key_index_entry<Interface, Cardinality, Entries>::type;

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct typed_key_index_entry_for;

template <typename Interface, typename Key, typename Cardinality>
struct typed_key_index_entry_for<Interface, Key, Cardinality, type_list<>> {
  using type = void;
};

template <typename Interface, typename Key, typename Cardinality, typename Head,
          typename... Tail>
struct typed_key_index_entry_for<Interface, Key, Cardinality,
                                 type_list<Head, Tail...>> {
private:
  static constexpr bool exact =
      std::is_same_v<typename Head::interface_type, Interface> &&
      std::is_same_v<typename Head::key_domain, ::dingo::typed_key<Key>> &&
      std::is_same_v<typename Head::cardinality, Cardinality>;

public:
  using type = std::conditional_t<
      exact, Head,
      typename typed_key_index_entry_for<Interface, Key, Cardinality,
                                         type_list<Tail...>>::type>;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
struct selected_typed_key_index_entry {
private:
  using normalized_interface = normalized_index_interface_t<Interface>;
  using normalized_key = normalized_index_key_t<Key>;

public:
  using type =
      typename typed_key_index_entry_for<normalized_interface, normalized_key,
                                         Cardinality, Entries>::type;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Entries>
using selected_typed_key_index_entry_t =
    typename selected_typed_key_index_entry<Interface, Key, Cardinality,
                                            Entries>::type;

template <typename Value, typename Allocator>
using selector_storage_allocator_t = std::conditional_t<
    ::dingo::is_static_allocator_v<Allocator>, std::allocator<Value>,
    typename std::allocator_traits<Allocator>::template rebind_alloc<Value>>;

template <typename StorageAllocator, typename Allocator>
StorageAllocator make_selector_storage_allocator(Allocator &allocator) {
  if constexpr (::dingo::is_static_allocator_v<Allocator>) {
    (void)allocator;
    return StorageAllocator{};
  } else {
    return StorageAllocator(allocator);
  }
}

template <typename Key, typename Value, typename Allocator>
class unique_selector_projection {
  using pair_type = std::pair<Key, Value>;
  using allocator_type = selector_storage_allocator_t<pair_type, Allocator>;
  using storage_type = std::vector<pair_type, allocator_type>;

public:
  using iterator = typename storage_type::iterator;

  explicit unique_selector_projection(Allocator &allocator)
      : entries_(make_selector_storage_allocator<allocator_type>(allocator)) {}

  std::pair<iterator, bool> emplace(Key key, Value value) {
    auto it = find(key);
    if (it != end()) {
      return {it, false};
    }
    entries_.emplace_back(std::move(key), std::move(value));
    return {std::prev(entries_.end()), true};
  }

  iterator find(const Key &key) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->first == key) {
        return it;
      }
    }
    return entries_.end();
  }

  iterator end() { return entries_.end(); }

private:
  storage_type entries_;
};

template <typename Key, typename Value, typename Allocator>
class multi_selector_projection {
  using value_allocator_type = selector_storage_allocator_t<Value, Allocator>;
  using bucket_type = std::vector<Value, value_allocator_type>;
  using pair_type = std::pair<Key, bucket_type>;
  using pair_allocator_type =
      selector_storage_allocator_t<pair_type, Allocator>;
  using storage_type = std::vector<pair_type, pair_allocator_type>;

public:
  using iterator = typename storage_type::iterator;

  explicit multi_selector_projection(Allocator &allocator)
      : value_allocator_(
            make_selector_storage_allocator<value_allocator_type>(allocator)),
        entries_(
            make_selector_storage_allocator<pair_allocator_type>(allocator)) {}

  iterator emplace(Key key, Value value) {
    auto it = find(key);
    if (it == end()) {
      entries_.emplace_back(std::move(key), bucket_type(value_allocator_));
      it = std::prev(entries_.end());
    }
    it->second.emplace_back(std::move(value));
    return it;
  }

  iterator find(const Key &key) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
      if (it->first == key) {
        return it;
      }
    }
    return entries_.end();
  }

  iterator end() { return entries_.end(); }

private:
  value_allocator_type value_allocator_;
  storage_type entries_;
};

struct no_key_selector_value {};

inline bool operator==(no_key_selector_value, no_key_selector_value) {
  return true;
}

struct typed_key_selector_value {};

inline bool operator==(typed_key_selector_value, typed_key_selector_value) {
  return true;
}

template <typename Entry, typename Value, typename Allocator,
          typename Enable = void>
struct index_holder;

template <typename Entry, typename Value, typename Allocator>
struct index_holder<
    Entry, Value, Allocator,
    std::enable_if_t<
        !std::is_same_v<typename Entry::key_domain,
                        ::dingo::runtime_key<index_key_domain_type_t<
                            typename Entry::key_domain>>> &&
        !std::is_same_v<typename Entry::key_domain,
                        ::dingo::typed_key<index_key_domain_type_t<
                            typename Entry::key_domain>>> &&
        !std::is_same_v<typename Entry::key_domain, ::dingo::no_key>>> {
  static_assert(index_dependent_false_v<Entry>,
                "unsupported dingo selector key domain");
};

template <typename Entry, typename Value, typename Allocator>
struct index_holder<
    Entry, Value, Allocator,
    std::enable_if_t<
        std::is_same_v<typename Entry::key_domain,
                       ::dingo::runtime_key<index_key_domain_type_t<
                           typename Entry::key_domain>>> &&
        std::is_same_v<typename Entry::cardinality, ::dingo::one>>> {
  using key_type = index_key_domain_type_t<typename Entry::key_domain>;
  using index_type = unique_selector_projection<key_type, Value, Allocator>;

  explicit index_holder(Allocator &allocator) : index_(allocator) {}

  index_type &get(Allocator &allocator) {
    if (!index_) {
      index_.emplace(allocator);
    }
    return *index_;
  }

private:
  allocator_ptr<index_type, Allocator> index_;
};

template <typename Entry, typename Value, typename Allocator>
struct index_holder<
    Entry, Value, Allocator,
    std::enable_if_t<
        std::is_same_v<typename Entry::key_domain,
                       ::dingo::runtime_key<index_key_domain_type_t<
                           typename Entry::key_domain>>> &&
        std::is_same_v<typename Entry::cardinality, ::dingo::many>>> {
  using key_type = index_key_domain_type_t<typename Entry::key_domain>;
  using index_type = multi_selector_projection<key_type, Value, Allocator>;

  explicit index_holder(Allocator &allocator) : index_(allocator) {}

  index_type &get(Allocator &allocator) {
    if (!index_) {
      index_.emplace(allocator);
    }
    return *index_;
  }

private:
  allocator_ptr<index_type, Allocator> index_;
};

template <typename Entry, typename Value, typename Allocator>
struct index_holder<
    Entry, Value, Allocator,
    std::enable_if_t<
        std::is_same_v<typename Entry::key_domain, ::dingo::no_key> &&
        std::is_same_v<typename Entry::cardinality, ::dingo::one>>> {
  using index_type =
      unique_selector_projection<no_key_selector_value, Value, Allocator>;

  explicit index_holder(Allocator &allocator) : index_(allocator) {}

  index_type &get(Allocator &allocator) {
    if (!index_) {
      index_.emplace(allocator);
    }
    return *index_;
  }

private:
  allocator_ptr<index_type, Allocator> index_;
};

template <typename Entry, typename Value, typename Allocator>
struct index_holder<
    Entry, Value, Allocator,
    std::enable_if_t<
        std::is_same_v<typename Entry::key_domain, ::dingo::no_key> &&
        std::is_same_v<typename Entry::cardinality, ::dingo::many>>> {
  using index_type =
      multi_selector_projection<no_key_selector_value, Value, Allocator>;

  explicit index_holder(Allocator &allocator) : index_(allocator) {}

  index_type &get(Allocator &allocator) {
    if (!index_) {
      index_.emplace(allocator);
    }
    return *index_;
  }

private:
  allocator_ptr<index_type, Allocator> index_;
};

template <typename Entry, typename Value, typename Allocator>
struct index_holder<
    Entry, Value, Allocator,
    std::enable_if_t<
        std::is_same_v<typename Entry::key_domain,
                       ::dingo::typed_key<index_key_domain_type_t<
                           typename Entry::key_domain>>> &&
        std::is_same_v<typename Entry::cardinality, ::dingo::one>>> {
  using index_type =
      unique_selector_projection<typed_key_selector_value, Value, Allocator>;

  explicit index_holder(Allocator &allocator) : index_(allocator) {}

  index_type &get(Allocator &allocator) {
    if (!index_) {
      index_.emplace(allocator);
    }
    return *index_;
  }

private:
  allocator_ptr<index_type, Allocator> index_;
};

template <typename Entry, typename Value, typename Allocator>
struct index_holder<
    Entry, Value, Allocator,
    std::enable_if_t<
        std::is_same_v<typename Entry::key_domain,
                       ::dingo::typed_key<index_key_domain_type_t<
                           typename Entry::key_domain>>> &&
        std::is_same_v<typename Entry::cardinality, ::dingo::many>>> {
  using index_type =
      multi_selector_projection<typed_key_selector_value, Value, Allocator>;

  explicit index_holder(Allocator &allocator) : index_(allocator) {}

  index_type &get(Allocator &allocator) {
    if (!index_) {
      index_.emplace(allocator);
    }
    return *index_;
  }

private:
  allocator_ptr<index_type, Allocator> index_;
};

template <typename Entries, typename Value, typename Allocator>
struct index_bindings_impl;

template <typename Value, typename Allocator>
struct index_bindings_impl<type_list<>, Value, Allocator> {
  index_bindings_impl(Allocator &) {}

  template <typename Interface, typename Key> void get_index(Allocator &) {
    static_assert(index_dependent_false_v<Interface, Key>,
                  "indexed registration or lookup has no matching "
                  "dingo selector definition for interface/key");
  }

  template <typename Interface, typename Cardinality>
  void get_no_key_index(Allocator &) {
    static_assert(index_dependent_false_v<Interface, Cardinality>,
                  "unkeyed registration or lookup has no matching "
                  "dingo no_key selector definition for interface/cardinality");
  }

  template <typename Interface, typename Key, typename Cardinality>
  void get_typed_key_index(Allocator &) {
    static_assert(
        index_dependent_false_v<Interface, Key, Cardinality>,
        "typed-key registration or lookup has no matching dingo typed_key "
        "selector definition for interface/key/cardinality");
  }
};

template <typename Value, typename Allocator, typename... Entries>
struct index_bindings_impl<type_list<Entries...>, Value, Allocator> {
  index_bindings_impl(Allocator &allocator)
      : projections_(index_holder<Entries, Value, Allocator>(allocator)...) {
    static_assert(!has_duplicate_index_projection_v<type_list<Entries...>>,
                  "duplicate dingo selector definition for interface/key "
                  "domain/cardinality");
    static_assert(!has_duplicate_index_slot_v<type_list<Entries...>>,
                  "conflicting dingo selector definitions for interface/key "
                  "domain");
  }

  template <typename Interface, typename Key>
  decltype(auto) get_index(Allocator &allocator) {
    using entry = selected_index_entry_t<Interface, Key, type_list<Entries...>>;
    if constexpr (std::is_void_v<entry>) {
      static_assert(!std::is_void_v<entry>,
                    "indexed registration or lookup has no matching "
                    "dingo selector definition for interface/key");
    } else {
      return std::get<index_holder<entry, Value, Allocator>>(projections_)
          .get(allocator);
    }
  }

  template <typename Interface, typename Cardinality>
  decltype(auto) get_no_key_index(Allocator &allocator) {
    using entry = selected_no_key_index_entry_t<Interface, Cardinality,
                                                type_list<Entries...>>;
    if constexpr (std::is_void_v<entry>) {
      static_assert(
          !std::is_void_v<entry>,
          "unkeyed registration or lookup has no matching dingo no_key "
          "selector definition for interface/cardinality");
    } else {
      return std::get<index_holder<entry, Value, Allocator>>(projections_)
          .get(allocator);
    }
  }

  template <typename Interface, typename Key, typename Cardinality>
  decltype(auto) get_typed_key_index(Allocator &allocator) {
    using entry = selected_typed_key_index_entry_t<Interface, Key, Cardinality,
                                                   type_list<Entries...>>;
    if constexpr (std::is_void_v<entry>) {
      static_assert(
          !std::is_void_v<entry>,
          "typed-key registration or lookup has no matching dingo typed_key "
          "selector definition for interface/key/cardinality");
    } else {
      return std::get<index_holder<entry, Value, Allocator>>(projections_)
          .get(allocator);
    }
  }

private:
  std::tuple<index_holder<Entries, Value, Allocator>...> projections_;
};

template <typename Definitions, typename Value, typename Allocator>
struct index_bindings
    : index_bindings_impl<normalize_index_definitions_t<Definitions>, Value,
                          Allocator> {
  using base = index_bindings_impl<normalize_index_definitions_t<Definitions>,
                                   Value, Allocator>;
  using base::base;
};

} // namespace detail

} // namespace dingo
