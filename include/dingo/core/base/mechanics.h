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
#include <type_traits>
#include <utility>

// Places a function in a cold section, where the compiler supports it.
#if defined(__GNUC__) || defined(__clang__)
#define DINGO_CORE_COLD __attribute__((cold))
#else
#define DINGO_CORE_COLD
#endif

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

// Whether the call is evaluated in a constant expression.
constexpr bool constant_evaluated() {
#if defined(__cpp_lib_is_constant_evaluated)
  return std::is_constant_evaluated();
#else
  return __builtin_is_constant_evaluated();
#endif
}

// The name of a type as the compiler spells it in a function signature,
// usable in constant evaluation; distinct types of a translation unit have
// distinct names.
template <typename Type> constexpr const char *type_name() {
#if defined(_MSC_VER) && !defined(__clang__)
  return __FUNCSIG__;
#else
  return __PRETTY_FUNCTION__;
#endif
}

// The tag whose address identifies a type.
template <typename Type> struct type_tag {
  static constexpr char value = 0;
};

namespace detail {
struct address_probe_first {};
struct address_probe_second {};

template <bool> struct constant_bool {};

template <typename First, typename Second, typename = void>
struct compares_addresses : std::false_type {};

template <typename First, typename Second>
struct compares_addresses<First, Second,
                          decltype(void(
                              constant_bool<(&type_tag<First>::value !=
                                             &type_tag<Second>::value)>{}))>
    : std::true_type {};
} // namespace detail

// spec: variant_points.named_type_id
#if defined(DINGO_VARIANT_NAMED_TYPE_ID)
inline constexpr bool named_type_ids = DINGO_VARIANT_NAMED_TYPE_ID;
#else
inline constexpr bool named_type_ids =
    !detail::compares_addresses<detail::address_probe_first,
                                detail::address_probe_second>::value;
#endif

} // namespace dingo::core
