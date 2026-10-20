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

#include <dingo/core/api/registration.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/engine/lookup.h>
#include <dingo/core/projection/key.h>

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace dingo {

struct one {};
struct many {};

struct ordered {};
struct unordered {};
template <std::size_t Size> struct array {};

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

// The index structure of a backend, and the bound of its key values, the
// size of a dense array. A backend of the user, which names its storage, is
// indexed in order: the index structure never changes which bindings a
// request names.
template <typename Backend> struct backend_index {
  static constexpr core::index_structure index = core::index_structure::ordered;
  static constexpr std::size_t bound = core::unbounded;
};

template <> struct backend_index<unordered> {
  static constexpr core::index_structure index =
      core::index_structure::unordered;
  static constexpr std::size_t bound = core::unbounded;
};

template <std::size_t Size> struct backend_index<array<Size>> {
  static constexpr core::index_structure index =
      core::index_structure::dense_array;
  static constexpr std::size_t bound = Size;
};

template <typename Interface, typename Key, typename Cardinality,
          typename Backend = ordered>
using lookup_declaration_t =
    core::lookup_declaration<typename lookup_interfaces<Interface>::type, Key,
                             std::is_same_v<Cardinality, many>,
                             backend_index<Backend>::index,
                             backend_index<Backend>::bound>;

template <typename Key, typename Interface, typename Cardinality,
          typename Backend>
struct associative_declaration {
  static_assert(backend_index<Backend>::bound == core::unbounded ||
                    std::is_integral_v<Key> || std::is_enum_v<Key>,
                "dingo::array<N> associative backend requires an integral "
                "or enum key");
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
struct base : core::lookup_declaration<core::type_list<>, core::no_key,
                                       std::is_same_v<Cardinality, many>,
                                       detail::backend_index<Backend>::index,
                                       detail::backend_index<Backend>::bound> {
};

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
