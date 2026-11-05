//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/lookup/ordered.h>
#include <dingo/lookup/tags.h>
#include <dingo/type/key.h>

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>

namespace dingo {
template <typename Cardinality = one, typename Backend = ordered> struct base {
  static_assert(std::is_same_v<Cardinality, one> ||
                    std::is_same_v<Cardinality, many>,
                "dingo::base requires dingo::one or dingo::many "
                "cardinality");
  using cardinality = Cardinality;
  using backend_type = Backend;
};

namespace detail {
template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Backend>
struct lookup_entry;

template <typename Rtti> struct base_lookup_key {
  using type_index = typename Rtti::type_index;

  type_index interface_type;
  type_index key_type;

  bool operator<(const base_lookup_key &other) const {
    return std::tie(interface_type, key_type) <
           std::tie(other.interface_type, other.key_type);
  }

  bool operator==(const base_lookup_key &other) const {
    return interface_type == other.interface_type && key_type == other.key_type;
  }
};

template <typename Rtti> struct base_lookup_interface {};

template <typename Rtti, typename Cardinality, typename Backend>
using base_lookup_entry =
    lookup_entry<base_lookup_interface<Rtti>,
                 lookup_key<key_value<base_lookup_key<Rtti>>>, Cardinality,
                 Backend>;

template <typename Definition>
struct is_base_lookup_definition : std::false_type {};

template <typename Cardinality, typename Backend>
struct is_base_lookup_definition<::dingo::base<Cardinality, Backend>>
    : std::true_type {};

// The base definitions among lookups<...>; there is at most one.
template <typename Definitions> struct base_lookup_definitions;

template <typename... Definitions>
struct base_lookup_definitions<::dingo::lookups<Definitions...>> {
  using type = type_list_cat_t<
      std::conditional_t<is_base_lookup_definition<Definitions>::value,
                         type_list<Definitions>, type_list<>>...>;
};

template <typename Definitions>
inline constexpr bool has_duplicate_base_lookup_definition_v =
    type_list_size_v<typename base_lookup_definitions<Definitions>::type> > 1;

// The base definition, or one singular ordered base when none is declared.
template <typename Definitions,
          typename Bases = typename base_lookup_definitions<Definitions>::type>
struct base_lookup_definition {
  using type = type_list_head_t<Bases>;
};

template <typename Definitions>
struct base_lookup_definition<Definitions, type_list<>> {
  using type = ::dingo::base<::dingo::one, ::dingo::ordered>;
};

template <typename Definitions>
using base_lookup_definition_t =
    typename base_lookup_definition<Definitions>::type;
} // namespace detail

} // namespace dingo

namespace std {
template <typename Rtti> struct hash<dingo::detail::base_lookup_key<Rtti>> {
  std::size_t
  operator()(const dingo::detail::base_lookup_key<Rtti> &key) const {
    using lookup_type_index =
        typename dingo::detail::base_lookup_key<Rtti>::type_index;
    const auto interface_hash =
        std::hash<lookup_type_index>{}(key.interface_type);
    const auto key_hash = std::hash<lookup_type_index>{}(key.key_type);
    return interface_hash ^ (key_hash + 0x9e3779b9U + (interface_hash << 6U) +
                             (interface_hash >> 2U));
  }
};
} // namespace std
