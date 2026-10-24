//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The declarations of constructor detection, shared by its backends: the kinds
// of a detected constructor, the detection modes and backends, and the
// arguments that probe a constructor.
//
// A detection mode says what a detection reports. constructor_arity reports
// the kind and the arity of the detected constructor, and
// constructor_signature additionally recovers its argument types.
//
// A backend is one implementation of the detection. The portable backend probes
// every arity with one constant expression. The msvc backend probes each arity
// with a class, searches the arities without a zero case to decrement, takes
// the arity of a std::array from its size, and recovers a signature by one
// probe per argument and conversion category. Both are standard C++, and both
// are tested on every compiler that accepts them.

#include <dingo/base/mechanics.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dingo {

enum class constructor_kind : std::uint8_t { concrete, generic, invalid };

// Reports the kind and the arity of the detected constructor.
struct constructor_arity {};

// Additionally recovers the argument types of the detected constructor.
struct constructor_signature {};

struct portable_constructor_detection {};
struct msvc_constructor_detection {};

// The backend of a detection that names none.
using default_constructor_detection =
    std::conditional_t<msvc_constructor_detection_variant,
                       msvc_constructor_detection,
                       portable_constructor_detection>;

namespace detail {
template <typename Type, typename = void>
struct is_complete : std::false_type {};

template <typename Type>
struct is_complete<Type, std::void_t<decltype(sizeof(Type))>> : std::true_type {
};

// Converts to any parameter type except `Disabled`, the constructed type.
template <typename Disabled> struct probe_argument {
  // Non-scalar wrappers may use constrained converting constructors that
  // require an xvalue source.
  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>> &&
                               !std::is_scalar_v<Type> &&
                               is_complete<std::decay_t<Type>>::value>>
  operator Type &&() const;

  // Reference probes stay available for incomplete types, which a lookup can
  // borrow without constructing them.
  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>>>>
  operator const Type &() const;

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>>>>
  operator Type &() const;

  // A prvalue covers both value and rvalue reference parameters.
  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>> &&
                               is_complete<std::decay_t<Type>>::value>>
  operator Type();
};

// Converts to nothing; a type constructible from it accepts any argument.
struct opaque_argument {};

template <typename Argument, std::size_t> using repeated = Argument;

inline constexpr std::size_t invalid_arity = static_cast<std::size_t>(-1);

// The arity a backend finds for `Type` in [0, Arity], or invalid_arity, and
// whether the constructor of that arity accepts any argument. Each backend
// specializes it.
template <typename Type, typename Backend, std::size_t Arity, typename = void>
struct constructor_search;

// The search of a scalar, which is value-initialized.
struct scalar_search {
  static constexpr std::size_t found = 0;
  static constexpr bool generic = false;
};

template <typename Type, typename Backend, std::size_t Arity>
using constructor_search_t =
    std::conditional_t<std::is_scalar_v<Type>, scalar_search,
                       constructor_search<Type, Backend, Arity>>;

template <typename Search>
inline constexpr constructor_kind search_kind =
    Search::found == invalid_arity ? constructor_kind::invalid
    : Search::generic              ? constructor_kind::generic
                                   : constructor_kind::concrete;

template <typename Search>
inline constexpr std::size_t search_arity =
    Search::found == invalid_arity ? 0 : Search::found;

// The argument types a backend recovers for the concrete constructor of
// `Type` of arity `Arity`, or void. Each backend specializes it.
template <typename Type, typename Backend, std::size_t Arity>
struct constructor_signature_recovery;

// An argument type recovered from a value conversion that bound an rvalue
// reference parameter is requested as the value.
template <typename Type> struct signature_dependency {
  using type = Type;
};

template <typename Type> struct signature_dependency<Type &&> {
  using type = Type;
};

template <typename Type>
using signature_dependency_t = typename signature_dependency<Type>::type;

template <typename Type> struct signature_result {
  using type = Type;
};
} // namespace detail

} // namespace dingo
