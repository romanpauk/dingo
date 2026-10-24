//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The signature recovery of the portable backend of constructor detection.
//
// The arity search proves that a type is constructible from N probe arguments,
// but not which parameter types overload resolution selects. Signature
// recovery probes the constructor of the detected arity once more, with an
// indexed argument per parameter. When overload resolution selects, for
// example, `operator U &()` for the argument of index I, it instantiates the
// exception specification of that conversion, which defines the friend of the
// argument with `U &` as its result. Querying the friend of each argument then
// recovers `type_list<Args...>`.
//
// The compilers of this recovery instantiate only the exception specification
// of the selected conversion; MSVC instantiates those of the competing
// conversions too and recovers a signature with the msvc backend.

#include <dingo/base/mechanics.h>
#include <dingo/constructor/constructor_arity.h>
#include <dingo/constructor/constructor_probe.h>

#include <cstddef>
#include <type_traits>
#include <utility>

// spec: variant_points.constructor_detection_msvc
#if DINGO_PORTABLE_CONSTRUCTOR_SIGNATURE

namespace dingo::detail {

DINGO_RECORDING_FRIENDS_BEGIN

// Instantiated by the exception specification of a selected conversion: it
// records `Dependency` as the result of the friend of `Argument`.
template <typename Dependency, typename Argument> struct signature_definition {
  friend auto signature_argument_type(Argument) {
    return signature_result<Dependency>{};
  }

  static constexpr bool value = true;
};

// The argument of index `Index` of the probe `Probe`, which makes the argument
// and its friend unique.
template <typename Disabled, typename Probe, std::size_t Index>
struct signature_argument {
  // Declares the friend that the selected conversion defines, so that a
  // query finds it by argument-dependent lookup.
  friend auto signature_argument_type(signature_argument);

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>> &&
                               is_complete<std::decay_t<Type>>::value>>
  operator Type &&() const
      noexcept(signature_definition<Type &&, signature_argument>::value);

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>>>>
  operator const Type &() const
      noexcept(signature_definition<const Type &, signature_argument>::value);

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>>>>
  operator Type &() const
      noexcept(signature_definition<Type &, signature_argument>::value);

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>> &&
                               is_complete<std::decay_t<Type>>::value>>
  operator Type() noexcept(
      signature_definition<Type, signature_argument>::value);
};

DINGO_RECORDING_FRIENDS_END

template <typename Type, typename Probe, typename Sequence, typename = void>
struct signature_arguments {
  using type = void;
};

template <typename Type, typename Probe, std::size_t... Indices>
struct signature_arguments<
    Type, Probe, std::index_sequence<Indices...>,
    std::void_t<decltype(signature_argument_type(
        signature_argument<Type, Probe, Indices>{}))...>> {
  using type = type_list<
      signature_dependency_t<typename decltype(signature_argument_type(
          signature_argument<Type, Probe, Indices>{}))::type>...>;
};

template <typename Type, typename Probe, typename Sequence>
struct signature_probe;

template <typename Type, typename Probe, std::size_t... Indices>
struct signature_probe<Type, Probe, std::index_sequence<Indices...>>
    : is_list_initializable_impl<Type, void,
                                 signature_argument<Type, Probe, Indices>...> {
};

// The probe of one type and arity, so that the friends of its arguments are
// defined once.
template <typename Type, std::size_t Arity> struct signature_probe_id {};

template <typename Type, std::size_t Arity>
struct constructor_signature_recovery<Type, portable_constructor_detection,
                                      Arity> {
private:
  using probe = signature_probe_id<Type, Arity>;
  static constexpr bool resolved =
      signature_probe<Type, probe, std::make_index_sequence<Arity>>::value;

public:
  using type = std::conditional_t<
      resolved,
      typename signature_arguments<Type, probe,
                                   std::make_index_sequence<Arity>>::type,
      void>;
};

} // namespace dingo::detail

#endif
