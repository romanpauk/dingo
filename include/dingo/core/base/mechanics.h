//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The tier-agnostic primitives of the next implementation. This is the only
// header with language-tier and compiler conditionals; every other header of
// the next implementation uses the primitives below. The generated
// variants.gen.h selects each tier from the capabilities of the compiler, and a
// build overrides a selection by defining its DINGO_VARIANT_* macro.

#include <dingo/core/base/config.h>
#include <dingo/core/base/variants.gen.h>

#include <cstddef>
#include <utility>

namespace dingo::core {

template <typename... Types> struct type_list {
  static constexpr std::size_t size = sizeof...(Types);
};

// False, but only when instantiated, for a static_assert in a template.
template <typename...> inline constexpr bool dependent_false = false;

// The element of a type pack at an index, selected without recursion.

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
template <std::size_t Index, typename... Types>
using pack_at_portable_t = typename decltype(detail::pack_select<Index>(
    detail::pack_indexer<std::index_sequence_for<Types...>, Types...>{}))::type;

#if DINGO_HAS_TYPE_PACK_ELEMENT
template <std::size_t Index, typename... Types>
using pack_at_builtin_t = __type_pack_element<Index, Types...>;
#endif

#if DINGO_HAS_PACK_INDEXING
// Clang offers pack indexing as an extension before C++26 and warns about it.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wc++26-extensions"
#endif
template <std::size_t Index, typename... Types>
using pack_at_indexing_t = Types...[Index];
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

#if DINGO_VARIANT_PACK_AT_INDEXING
template <std::size_t Index, typename... Types>
using pack_at_t = pack_at_indexing_t<Index, Types...>;
#elif DINGO_VARIANT_PACK_AT_BUILTIN
template <std::size_t Index, typename... Types>
using pack_at_t = pack_at_builtin_t<Index, Types...>;
#else
template <std::size_t Index, typename... Types>
using pack_at_t = pack_at_portable_t<Index, Types...>;
#endif

template <std::size_t Index, typename List> struct type_list_at;

template <std::size_t Index, typename... Types>
struct type_list_at<Index, type_list<Types...>> {
  using type = pack_at_t<Index, Types...>;
};

template <std::size_t Index, typename List>
using type_list_at_t = typename type_list_at<Index, List>::type;

} // namespace dingo::core
