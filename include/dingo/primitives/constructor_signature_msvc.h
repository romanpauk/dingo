//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The signature recovery of the msvc backend of constructor detection.
//
// MSVC instantiates the exception specifications of the competing conversions
// of an argument, so the one recording argument of the portable recovery could
// record several types for one parameter. This recovery probes each parameter
// and each conversion category separately: the target parameter gets an
// argument with the one conversion of its category, and the other parameters
// keep probe arguments, so that overload resolution still selects the same
// constructor. A key per probe isolates its friend, a marker friend makes a
// repeated definition a no-op, and the results of the categories combine into
// the type of the parameter. It is standard C++ and is tested on every
// compiler.

#include <dingo/base/mechanics.h>
#include <dingo/primitives/constructor_arity_msvc.h>
#include <dingo/primitives/constructor_probe.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::core::detail {

struct lvalue_probe {};
struct rvalue_probe {};
struct value_probe {};

DINGO_CORE_RECORDING_FRIENDS_BEGIN

// The key of a probe, apart from its argument: MSVC can canonicalize the name
// of an argument across specializations, while these explicit arguments keep
// the signature of every friend distinct.
template <typename Type, typename Probe, std::size_t Index>
struct category_key {
  friend auto category_type(category_key);
  friend constexpr int category_defined(category_key);
};

template <typename Dependency, typename Key, bool Defined>
struct category_definition_impl;

// MSVC can instantiate the same category through more than one conversion.
// The marker friend makes only the first instantiation define the type friend
// of the key.
template <typename Dependency, typename Key>
struct category_definition_impl<Dependency, Key, false> {
  friend auto category_type(Key) { return signature_result<Dependency>{}; }

  friend constexpr int category_defined(Key) { return 0; }

  static constexpr bool value = true;
};

template <typename Dependency, typename Key>
struct category_definition_impl<Dependency, Key, true> {
  static constexpr bool value = true;
};

DINGO_CORE_RECORDING_FRIENDS_END

template <typename Dependency, typename Type, typename Probe, std::size_t Index>
struct category_definition {
private:
  using key = category_key<Type, Probe, Index>;

  template <typename> static auto is_defined(...) -> std::false_type;

  template <typename, int = category_defined(key{})>
  static auto is_defined(int) -> std::true_type;

  using definition =
      category_definition_impl<Dependency, key,
                               decltype(is_defined<Dependency>(0))::value>;

public:
  static constexpr bool value = definition::value;
};

template <typename Disabled, typename Probe, std::size_t Index,
          typename Category>
struct category_argument;

template <typename Disabled, typename Probe, std::size_t Index>
struct category_argument<Disabled, Probe, Index, lvalue_probe> {
  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>>>>
  operator Type &() const
      noexcept(category_definition<Type &, Disabled, Probe, Index>::value);
};

template <typename Disabled, typename Probe, std::size_t Index>
struct category_argument<Disabled, Probe, Index, rvalue_probe> {
  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>> &&
                               is_complete<std::decay_t<Type>>::value>>
  operator Type &&() const
      noexcept(category_definition<Type &&, Disabled, Probe, Index>::value);
};

template <typename Disabled, typename Probe, std::size_t Index>
struct category_argument<Disabled, Probe, Index, value_probe> {
  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>> &&
                               is_complete<std::decay_t<Type>>::value>>
  operator Type() noexcept(
      category_definition<Type, Disabled, Probe, Index>::value);
};

template <typename Type, typename Probe, typename Category, std::size_t Target,
          typename Sequence>
struct category_probe;

template <typename Type, typename Probe, typename Category, std::size_t Target,
          std::size_t... Indices>
struct category_probe<Type, Probe, Category, Target,
                      std::index_sequence<Indices...>>
    : msvc_constructible<
          Type,
          std::conditional_t<Indices == Target,
                             category_argument<Type, Probe, Indices, Category>,
                             probe_argument<Type>>...> {};

template <typename Type, std::size_t Arity, std::size_t Target,
          typename Category>
struct category_probe_id {};

// The type that the conversion of `Category` gives the parameter of index
// `Target`, or void.
template <typename Type, std::size_t Arity, std::size_t Target,
          typename Category,
          bool = category_probe<
              Type, category_probe_id<Type, Arity, Target, Category>, Category,
              Target, std::make_index_sequence<Arity>>::value>
struct category_result {
  using type = void;
};

template <typename Type, std::size_t Arity, std::size_t Target,
          typename Category>
struct category_result<Type, Arity, Target, Category, true> {
private:
  using probe = category_probe_id<Type, Arity, Target, Category>;

public:
  using type = typename decltype(category_type(
      category_key<Type, probe, Target>{}))::type;
};

// The parameter type that the lvalue and rvalue conversions give: a const
// lvalue reference, else the value an rvalue reference binds, else the lvalue
// reference.
template <typename Lvalue, typename Rvalue> struct reference_argument {
private:
  static constexpr bool is_const_lvalue =
      std::is_lvalue_reference_v<Lvalue> &&
      std::is_const_v<std::remove_reference_t<Lvalue>>;

public:
  using type = std::conditional_t<
      is_const_lvalue, Lvalue,
      std::conditional_t<!std::is_void_v<Lvalue> && !std::is_void_v<Rvalue>,
                         std::remove_reference_t<Rvalue>,
                         std::conditional_t<!std::is_void_v<Lvalue>, Lvalue,
                                            std::remove_reference_t<Rvalue>>>>;
};

template <typename Type, std::size_t Arity, std::size_t Target,
          typename Lvalue =
              typename category_result<Type, Arity, Target, lvalue_probe>::type,
          typename Rvalue =
              typename category_result<Type, Arity, Target, rvalue_probe>::type,
          bool Missing = std::is_void_v<Lvalue> && std::is_void_v<Rvalue>,
          bool LvalueOnly = !std::is_void_v<Lvalue> && std::is_void_v<Rvalue>>
struct parameter_argument : reference_argument<Lvalue, Rvalue> {};

// Neither reference converts, so the parameter is a value that only a prvalue
// initializes.
template <typename Type, std::size_t Arity, std::size_t Target, typename Lvalue,
          typename Rvalue>
struct parameter_argument<Type, Arity, Target, Lvalue, Rvalue, true, false>
    : category_result<Type, Arity, Target, value_probe> {};

// Only an lvalue converts: a value parameter that a copy initializes, or an
// lvalue reference.
template <typename Type, std::size_t Arity, std::size_t Target, typename Lvalue,
          typename Rvalue>
struct parameter_argument<Type, Arity, Target, Lvalue, Rvalue, false, true> {
private:
  using value =
      typename category_result<Type, Arity, Target, value_probe>::type;

public:
  using type =
      std::conditional_t<!std::is_void_v<value>, value,
                         typename reference_argument<Lvalue, Rvalue>::type>;
};

template <typename... Args> struct parameter_list {
  using type = std::conditional_t<(std::is_void_v<Args> || ...), void,
                                  type_list<signature_dependency_t<Args>...>>;
};

template <typename Type, std::size_t Arity, typename Sequence>
struct msvc_signature_recovery;

template <typename Type, std::size_t Arity, std::size_t... Indices>
struct msvc_signature_recovery<Type, Arity, std::index_sequence<Indices...>>
    : parameter_list<
          typename parameter_argument<Type, Arity, Indices>::type...> {};

template <typename Type, std::size_t Arity>
struct constructor_signature_recovery<Type, msvc_constructor_detection, Arity>
    : msvc_signature_recovery<Type, Arity, std::make_index_sequence<Arity>> {};

} // namespace dingo::core::detail
