//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <cstddef>
#include <initializer_list>
#include <tuple>
#include <type_traits>

#if !DINGO_VARIANT_PACK_AT_INDEXING && !DINGO_VARIANT_PACK_AT_BUILTIN
#include <utility>
#endif

namespace dingo {
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

#if DINGO_VARIANT_PACK_AT_INDEXING
// Clang offers pack indexing as an extension before C++26 and warns about it.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wc++26-extensions"
#endif
template <typename List, std::size_t Index> struct type_list_at;

template <std::size_t Index, typename... Types>
struct type_list_at<type_list<Types...>, Index> {
  using type = Types...[Index];
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#elif DINGO_VARIANT_PACK_AT_BUILTIN
template <typename List, std::size_t Index> struct type_list_at;

template <std::size_t Index, typename... Types>
struct type_list_at<type_list<Types...>, Index> {
  using type = __type_pack_element<Index, Types...>;
};
#else
namespace detail {
template <std::size_t Index, typename Type> struct pack_indexed {
  using type = Type;
};

template <typename Indices, typename... Types> struct pack_indexer;

template <std::size_t... Indices, typename... Types>
struct pack_indexer<std::index_sequence<Indices...>, Types...>
    : pack_indexed<Indices, Types>... {};

template <std::size_t Index, typename Type>
pack_indexed<Index, Type> pack_select(const pack_indexed<Index, Type> &);
} // namespace detail

// The portable selection: overload resolution picks the base class with the
// index, which is one instantiation per pack rather than one per element.
template <typename List, std::size_t Index> struct type_list_at;

template <std::size_t Index, typename... Types>
struct type_list_at<type_list<Types...>, Index> {
  using type = typename decltype(detail::pack_select<Index>(
      detail::pack_indexer<std::index_sequence_for<Types...>,
                           Types...>{}))::type;
};
#endif

template <typename List, std::size_t Index>
using type_list_at_t = typename type_list_at<List, Index>::type;

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

template <typename T> struct to_type_list {
  using type = T;
};

template <typename... Types> struct to_type_list<std::tuple<Types...>> {
  using type = type_list<typename to_type_list<Types>::type...>;
};

template <typename T> using to_type_list_t = typename to_type_list<T>::type;

template <typename RTTI, typename Function, typename... Types>
bool for_type(type_list<Types...>, const typename RTTI::type_index &type,
              Function &&fn) {
  bool matched = false;
  (void)std::initializer_list<int>{
      (((!matched && RTTI::template get_type_index<Types>() == type)
            ? (fn(type_list_iterator<Types>{}), matched = true, 0)
            : 0))...};
  return matched;
}

template <typename Function, typename... Types>
void for_each(type_list<Types...>, Function &&fn) {
  (fn(type_list_iterator<Types>{}), ...);
}

} // namespace dingo
