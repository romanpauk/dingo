//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Constructor detection: the constructor of a type that a factory calls, found
// by a backend in a detection mode.
//
// constructor_detection<Type, Mode, Backend, Arity> searches the arities in
// [0, Arity] from the largest down. It reports the kind of the detected
// constructor and its arity: a constructor that accepts any argument, such as a
// constructor template, is generic, and a type with no constructor of a single
// largest arity is invalid. A scalar is value-initialized and takes no
// argument. `arguments` is the type_list of the argument types when the mode
// is constructor_signature and the constructor is concrete, the empty list for
// a concrete constructor without arguments, and void otherwise. The arguments
// of a std::array are its elements.
//
// The backend defaults to the one that
// variant_points.constructor_detection_msvc selects: msvc for MSVC, portable
// otherwise.

#include <dingo/base/config.h>
#include <dingo/base/mechanics.h>
#include <dingo/constructor/constructor_arity.h>
#include <dingo/constructor/constructor_arity_msvc.h>
#include <dingo/constructor/constructor_probe.h>
#include <dingo/constructor/constructor_signature.h>
#include <dingo/constructor/constructor_signature_msvc.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Type, std::size_t> using element = Type;

template <typename Element, typename Sequence> struct array_arguments;

template <typename Element, std::size_t... Indices>
struct array_arguments<Element, std::index_sequence<Indices...>> {
  using type = type_list<element<Element, Indices>...>;
};

template <typename Type, typename Backend, std::size_t Arity>
struct signature_of : constructor_signature_recovery<Type, Backend, Arity> {};

template <typename Element, std::size_t Size, typename Backend>
struct signature_of<std::array<Element, Size>, Backend, Size>
    : array_arguments<Element, std::make_index_sequence<Size>> {};

template <typename Type, typename Mode, typename Backend, constructor_kind Kind,
          std::size_t Arity>
struct detected_arguments {
  using type = void;
};

template <typename Type, typename Mode, typename Backend>
struct detected_arguments<Type, Mode, Backend, constructor_kind::concrete, 0> {
  using type = type_list<>;
};

template <typename Type, typename Backend>
struct detected_arguments<Type, constructor_signature, Backend,
                          constructor_kind::concrete, 0> {
  using type = type_list<>;
};

template <typename Type, typename Backend, std::size_t Arity>
struct detected_arguments<Type, constructor_signature, Backend,
                          constructor_kind::concrete, Arity> {
  using type = typename signature_of<Type, Backend, Arity>::type;
  static_assert(!std::is_void_v<type>,
                "constructor signature detection recovers the arguments of "
                "every concrete constructor that type_tree detection finds");
};
} // namespace detail

template <typename Type, typename Mode = constructor_arity,
          typename Backend = default_constructor_detection,
          std::size_t Arity = DINGO_CONSTRUCTOR_DETECTION_ARGS>
struct constructor_detection {
private:
  using search = detail::constructor_search_t<Type, Backend, Arity>;

public:
  static constexpr constructor_kind kind = detail::search_kind<search>;
  static constexpr std::size_t arity = detail::search_arity<search>;
  using arguments = typename detail::detected_arguments<Type, Mode, Backend,
                                                        kind, arity>::type;
};

} // namespace dingo
