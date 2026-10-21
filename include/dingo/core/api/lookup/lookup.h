//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/lookup/lookup.h of the next implementation: the lookup declarations a
// container's traits list, which decide whether a domain of an interface and
// a key holds one binding or many. The index structure a declaration names
// only accelerates the lookup and never changes which bindings a request
// names.

#include <dingo/core/api/lookup/indexes.h>
#include <dingo/core/api/registration.h>
#include <dingo/core/backend/index.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/lookup.h>
#include <dingo/core/projection/key.h>
#include <dingo/core/vocabulary/compile_diagnostics.gen.h>

#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <vector>

namespace dingo {

struct one {};
struct many {};

// A lookup backend indexes the key values of a domain. It is a type with a
// member template index<Value, Candidate>, an index as core/backend/index.h
// describes it, and it may name the bound of its key values, `bound`, the size
// of a dense array. The backends of the library are such types, as a backend
// of the user is.

// A candidate of a request with the key value it is found by, which the
// index of a backend takes.
template <typename Value, typename Candidate>
using index_item = core::index_item<Value, Candidate>;

// The key values in order.
struct ordered {
  template <typename Value, typename Candidate>
  using index = core::ordered_index_t<Value, Candidate>;
};

// The key values in a hash table.
struct unordered {
  template <typename Value, typename Candidate>
  using index = core::hashed_index_t<Value, Candidate>;
};

// The key values 0 up to Size, excluded, in a dense array.
template <std::size_t Size> struct array {
  static constexpr std::size_t bound = Size;
  template <typename Value, typename Candidate>
  using index = core::dense_index_t<Size, Value, Candidate>;
};

// The lookup declarations of container traits.
template <typename... Declarations> struct lookups {};

namespace detail {
template <typename Interface> struct lookup_interfaces {
  using type = core::type_list<Interface>;
};

template <typename... Interfaces>
struct lookup_interfaces<::dingo::interfaces<Interfaces...>> {
  using type = core::type_list<Interfaces...>;
};

// The indexing of a backend: the backend, which names its index.
template <typename Backend, typename = void>
struct names_index : std::false_type {};

template <typename Backend>
struct names_index<Backend, std::void_t<typename Backend::template index<
                                int, core::index_item<int, int>>>>
    : std::true_type {};

template <typename Backend> struct backend_indexing {
  static_assert(
      core::require<names_index<Backend>::value,
                    core::diag::lookup_backend_without_index, Backend>());
  using type = Backend;
};

// The bound of the key values of a backend, unbounded unless it names one.
template <typename Backend, typename = void> struct backend_bound {
  static constexpr std::size_t value = core::unbounded;
};

template <typename Backend>
struct backend_bound<Backend, std::void_t<decltype(Backend::bound)>> {
  static constexpr std::size_t value = Backend::bound;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Backend = ordered>
using lookup_declaration_t =
    core::lookup_declaration<typename lookup_interfaces<Interface>::type, Key,
                             std::is_same_v<Cardinality, many>,
                             typename backend_indexing<Backend>::type,
                             backend_bound<Backend>::value>;

template <typename Key, typename Interface, typename Cardinality,
          typename Backend>
struct associative_declaration {
  static_assert(
      core::require<backend_bound<Backend>::value == core::unbounded ||
                        std::is_integral_v<Key> || std::is_enum_v<Key>,
                    core::diag::dense_key_type, Interface, Key>());
  using type = lookup_declaration_t<Interface, core::runtime_key<Key>,
                                    Cardinality, Backend>;
};
} // namespace detail

// The declarations are class templates, so the arguments of one can be
// deduced; each is the lookup declaration it derives from.

// The bindings of an interface without a key, one of them.
template <typename Interface>
struct single : detail::lookup_declaration_t<Interface, core::no_key, one> {};

// The bindings of an interface without a key, any number of them.
template <typename Interface>
struct collection
    : detail::lookup_declaration_t<Interface, core::no_key, many> {};

// The bindings of an interface by a tag key.
template <typename Tag, typename Interface, typename Cardinality = one>
struct typed
    : detail::lookup_declaration_t<Interface, core::tag_key<Tag>, Cardinality> {
};

// The bindings of an interface by the value of a key type, fixed or supplied
// at runtime. A dense array<N> stores the values 0 up to N, excluded, of an
// integral or enumeration key type; a binding of a value outside it is
// rejected, and a request of one finds no binding.
template <typename Key, typename Interface, typename Cardinality = one,
          typename Backend = ordered>
struct associative
    : detail::associative_declaration<Key, Interface, Cardinality,
                                      Backend>::type {};

// Every domain no other declaration covers.
template <typename Cardinality = one, typename Backend = ordered>
struct base
    : core::lookup_declaration<core::type_list<>, core::no_key,
                               std::is_same_v<Cardinality, many>,
                               typename detail::backend_indexing<Backend>::type,
                               detail::backend_bound<Backend>::value> {};

} // namespace dingo

namespace dingo::core {

template <typename Lookups> struct lookup_list;

template <typename... Declarations>
struct lookup_list<::dingo::lookups<Declarations...>> {
  using type = type_list<Declarations...>;
};

template <typename... Declarations>
struct lookup_list<std::tuple<Declarations...>> {
  using type = type_list<Declarations...>;
};

// The lookup table the traits of a container declare.
template <typename Traits>
inline constexpr const lookup_table &lookup_table_v = lookup_table_of<
    typename lookup_list<typename Traits::lookup_definition_type>::type>::value;

} // namespace dingo::core
