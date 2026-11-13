//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
} // namespace detail

template <typename... Types> struct type_list {};
template <typename T> struct type_list_iterator {
  using type = T;
};

namespace detail {
template <typename Accumulated, typename... Lists> struct type_list_cat_impl;

template <typename... Accumulated>
struct type_list_cat_impl<type_list<Accumulated...>> {
  using type = type_list<Accumulated...>;
};

template <typename... Accumulated, typename... Head, typename... Tail>
struct type_list_cat_impl<type_list<Accumulated...>, type_list<Head...>,
                          Tail...>
    : type_list_cat_impl<type_list<Accumulated..., Head...>, Tail...> {};
} // namespace detail

template <typename... Lists> struct type_list_cat {
  using type = typename detail::type_list_cat_impl<type_list<>, Lists...>::type;
};

template <typename... Lists>
using type_list_cat_t = typename type_list_cat<Lists...>::type;

template <typename List> struct type_list_size;

template <typename... Types>
struct type_list_size<type_list<Types...>>
    : std::integral_constant<size_t, sizeof...(Types)> {};

template <typename List>
static constexpr size_t type_list_size_v = type_list_size<List>::value;

template <typename List> struct type_list_head;

template <typename Head, typename... Tail>
struct type_list_head<type_list<Head, Tail...>> {
  using type = Head;
};

template <typename List>
using type_list_head_t = typename type_list_head<List>::type;

namespace detail {
template <std::size_t Index, typename Type> struct pack_indexed {
  using type = Type;
};

template <typename Indexes, typename... Types> struct pack_indexer;

template <std::size_t... Indexes, typename... Types>
struct pack_indexer<std::index_sequence<Indexes...>, Types...>
    : pack_indexed<Indexes, Types>... {};

template <std::size_t Index, typename Type>
pack_indexed<Index, Type> pack_select(const pack_indexed<Index, Type> &);

// The type at Index of Types without the builtin: overload resolution selects
// the base of the index, so a pack takes one instantiation, not one for each
// type.
template <std::size_t Index, typename... Types>
using pack_at_portable_t = typename decltype(pack_select<Index>(
    pack_indexer<std::index_sequence_for<Types...>, Types...>{}))::type;
} // namespace detail

// The type at Index of Types.
#if DINGO_HAS_TYPE_PACK_ELEMENT
template <std::size_t Index, typename... Types>
using pack_at_t = __type_pack_element<Index, Types...>;
#else
template <std::size_t Index, typename... Types>
using pack_at_t = detail::pack_at_portable_t<Index, Types...>;
#endif

// The position of the first true of Matches, or the count of Matches.
template <bool... Matches> constexpr std::size_t pack_find() {
  constexpr bool matches[] = {Matches..., false};
  std::size_t index = 0;
  while (index < sizeof...(Matches) && !matches[index]) {
    ++index;
  }
  return index;
}

template <std::size_t Index, typename List> struct type_list_at;

template <std::size_t Index, typename... Types>
struct type_list_at<Index, type_list<Types...>> {
  using type = pack_at_t<Index, Types...>;
};

template <std::size_t Index, typename List>
using type_list_at_t = typename type_list_at<Index, List>::type;

// The first type of List whose match is true, or Default. Default is the type
// after the last, at the position pack_find gives when nothing matches.
template <typename Default, typename List, bool... Matches>
struct type_list_find;

template <typename Default, typename... Types, bool... Matches>
struct type_list_find<Default, type_list<Types...>, Matches...> {
  static_assert(sizeof...(Types) == sizeof...(Matches));
  using type = pack_at_t<pack_find<Matches...>(), Types..., Default>;
};

template <typename Default, typename List, bool... Matches>
using type_list_find_t =
    typename type_list_find<Default, List, Matches...>::type;

template <typename T, typename List> struct type_list_contains;

// Compare the pack directly so membership checks do not instantiate a new
// type_list and detector specialization for every suffix.
template <typename T, typename... Types>
struct type_list_contains<T, type_list<Types...>>
    : std::bool_constant<(std::is_same_v<T, Types> || ...)> {};

template <typename T, typename List>
inline constexpr bool type_list_contains_v = type_list_contains<T, List>::value;

namespace detail {
template <typename Accumulated, typename Remaining>
struct type_list_unique_impl;

template <typename... Accumulated>
struct type_list_unique_impl<type_list<Accumulated...>, type_list<>> {
  using type = type_list<Accumulated...>;
};

template <typename... Accumulated, typename Head, typename... Tail>
struct type_list_unique_impl<type_list<Accumulated...>,
                             type_list<Head, Tail...>>
    : type_list_unique_impl<
          std::conditional_t<
              type_list_contains_v<Head, type_list<Accumulated...>>,
              type_list<Accumulated...>, type_list<Accumulated..., Head>>,
          type_list<Tail...>> {};
} // namespace detail

template <typename List>
using type_list_unique_t =
    typename detail::type_list_unique_impl<type_list<>, List>::type;

template <typename Left, typename Right> struct type_list_merge {
  using type = type_list_unique_t<type_list_cat_t<Left, Right>>;
};

template <typename... Types>
struct type_list_merge<type_list<>, type_list<Types...>> {
  using type = type_list<Types...>;
};

template <typename... Types>
struct type_list_merge<type_list<Types...>, type_list<>> {
  using type = type_list<Types...>;
};

template <> struct type_list_merge<type_list<>, type_list<>> {
  using type = type_list<>;
};

template <typename Left, typename Right>
using type_list_merge_t = typename type_list_merge<Left, Right>::type;

template <typename Function, typename... Types>
void for_each(type_list<Types...>, Function &&fn) {
  (fn(type_list_iterator<Types>{}), ...);
}

namespace detail {
// How many times Selected is in List.
template <typename List, typename Selected> struct type_list_count;

template <typename Selected, typename... Alternatives>
struct type_list_count<type_list<Alternatives...>, Selected>
    : std::integral_constant<
          size_t,
          (0u + ... + (std::is_same_v<Selected, Alternatives> ? 1u : 0u))> {};

} // namespace detail

} // namespace dingo
