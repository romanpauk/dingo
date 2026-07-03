//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/ordered.h>
#include <dingo/lookup/tags.h>
#include <dingo/type/type_list.h>

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
    if (interface_type < other.interface_type) {
      return true;
    }
    if (other.interface_type < interface_type) {
      return false;
    }
    return key_type < other.key_type;
  }

  bool operator==(const base_lookup_key &other) const {
    return interface_type == other.interface_type && key_type == other.key_type;
  }
};

template <typename Rtti> struct base_lookup_interface {};

template <typename Rtti, typename Cardinality, typename Backend>
using base_lookup_entry =
    lookup_entry<base_lookup_interface<Rtti>,
                 ::dingo::runtime_key<base_lookup_key<Rtti>>, Cardinality,
                 Backend>;

template <typename Definition>
struct is_base_lookup_definition : std::false_type {};

template <typename Cardinality, typename Backend>
struct is_base_lookup_definition<::dingo::base<Cardinality, Backend>>
    : std::true_type {};

template <typename DefinitionList> struct base_lookup_definition_count;

template <typename... Definitions>
struct base_lookup_definition_count<type_list<Definitions...>>
    : std::integral_constant<
          std::size_t,
          (std::size_t{0} + ... +
           (is_base_lookup_definition<Definitions>::value ? 1U : 0U))> {};

template <>
struct base_lookup_definition_count<std::tuple<>>
    : std::integral_constant<std::size_t, 0> {};

template <typename... Definitions>
struct base_lookup_definition_count<::dingo::lookups<Definitions...>>
    : base_lookup_definition_count<type_list<Definitions...>> {};

template <typename Definitions>
inline constexpr bool has_duplicate_base_lookup_definition_v =
    base_lookup_definition_count<Definitions>::value > 1U;

template <typename DefinitionList> struct base_lookup_definition;

template <> struct base_lookup_definition<type_list<>> {
  using type = ::dingo::base<::dingo::one, ::dingo::ordered>;
};

template <typename Head, typename... Tail>
struct base_lookup_definition<type_list<Head, Tail...>> {
private:
  using tail_type = typename base_lookup_definition<type_list<Tail...>>::type;

public:
  using type = std::conditional_t<is_base_lookup_definition<Head>::value, Head,
                                  tail_type>;
};

template <>
struct base_lookup_definition<std::tuple<>>
    : base_lookup_definition<type_list<>> {};

template <typename... Definitions>
struct base_lookup_definition<::dingo::lookups<Definitions...>>
    : base_lookup_definition<type_list<Definitions...>> {};

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
