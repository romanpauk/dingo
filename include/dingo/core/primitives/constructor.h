//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Constructor detection: the constructor a factory calls for a type, found by
// probing it with arguments that convert to any parameter type, and its
// invocation with arguments that resolve exactly the type each parameter asks
// for. The detection searches from the largest arity down, so the constructor
// with the most parameters wins, and it ignores the copy constructor. A
// constructor that accepts any argument, such as a constructor template, is
// generic and needs an explicit factory. A type that declares its constructor,
// with a `dingo_constructor_type` naming its signature, is constructed by it,
// and a scalar is value-initialized.
//
// The technique is the one of the current implementation in
// dingo/factory/detail/constructor_detection.hpp, without its request
// decorations.

#include <dingo/core/base/config.h>
#include <dingo/core/base/mechanics.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace dingo::core {

enum class constructor_kind : std::uint8_t { concrete, generic, invalid };

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

template <typename Type, typename... Args>
using list_initialization_t = decltype(Type{std::declval<Args>()...});

template <typename Type, typename = void, typename... Args>
struct is_list_initializable_impl : std::false_type {};

template <typename Type, typename... Args>
struct is_list_initializable_impl<
    Type, std::void_t<list_initialization_t<Type, Args...>>, Args...>
    : std::bool_constant<sizeof...(Args) != 1 ||
                         (!std::is_same_v<Type, std::decay_t<Args>> && ...)> {};

template <typename Type, typename... Args>
inline constexpr bool is_list_initializable_v =
    is_list_initializable_impl<Type, void, Args...>::value;

template <typename Argument, std::size_t> using repeated = Argument;

template <typename Type, typename Argument, std::size_t... Indices>
constexpr bool constructible_with(std::index_sequence<Indices...>) {
  return is_list_initializable_v<Type, repeated<Argument, Indices>...>;
}

inline constexpr std::size_t invalid_arity = static_cast<std::size_t>(-1);

// The largest arity in [0, Arity] that constructs `Type`.
template <typename Type, std::size_t Arity> constexpr std::size_t find_arity() {
  if constexpr (constructible_with<Type, probe_argument<Type>>(
                    std::make_index_sequence<Arity>{})) {
    return Arity;
  } else if constexpr (Arity == 0) {
    return invalid_arity;
  } else {
    return find_arity<Type, Arity - 1>();
  }
}
} // namespace detail

namespace detail {
template <typename Signature> struct declared_signature {};

template <template <typename...> typename Template, typename Type,
          typename... Args>
struct declared_signature<Template<Type(Args...)>> {
  using arguments = type_list<Args...>;
};
} // namespace detail

// The argument types a type declares for its constructor.
template <typename Type, typename = void> struct declared_constructor {
  static constexpr bool declared = false;
};

template <typename Type>
struct declared_constructor<Type,
                            std::void_t<typename Type::dingo_constructor_type>>
    : detail::declared_signature<typename Type::dingo_constructor_type> {
  static constexpr bool declared = true;
};

// The constructor a factory calls for `Type`.
template <typename Type> struct detected_constructor {
private:
  static constexpr std::size_t found = [] {
    if constexpr (std::is_scalar_v<Type>) {
      return std::size_t(0);
    } else {
      return detail::find_arity<Type, DINGO_CONSTRUCTOR_DETECTION_ARGS>();
    }
  }();
  static constexpr bool generic = [] {
    if constexpr (found == detail::invalid_arity || found == 0) {
      return false;
    } else {
      return detail::constructible_with<Type, detail::opaque_argument>(
          std::make_index_sequence<found>{});
    }
  }();

public:
  static constexpr constructor_kind kind =
      found == detail::invalid_arity ? constructor_kind::invalid
      : generic                      ? constructor_kind::generic
                                     : constructor_kind::concrete;
  static constexpr std::size_t arity =
      found == detail::invalid_arity ? 0 : found;
};

// An argument that resolves the type its parameter asks for through
// `Resolver::resolve<Request>()`.
template <typename Disabled, typename Resolver> class constructor_argument {
public:
  explicit constructor_argument(Resolver &resolver) : resolver_(resolver) {}

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>>>>
  operator const Type &() const {
    return resolver_.template resolve<const Type &>();
  }

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>>>>
  operator Type &() const {
    return resolver_.template resolve<Type &>();
  }

  template <typename Type, typename = std::enable_if_t<
                               !std::is_same_v<Disabled, std::decay_t<Type>> &&
                               detail::is_complete<std::decay_t<Type>>::value>>
  operator Type() {
    return resolver_.template argument<Type>();
  }

private:
  Resolver &resolver_;
};

namespace detail {
template <typename Type, typename Resolver, typename Sink,
          std::size_t... Indices>
decltype(auto) invoke_detected(Resolver &resolver, Sink &&sink,
                               std::index_sequence<Indices...>) {
  return std::forward<Sink>(sink)(
      ((void)Indices, constructor_argument<Type, Resolver>(resolver))...);
}

template <typename Resolver, typename Sink, typename... Args>
decltype(auto) invoke_declared(Resolver &resolver, Sink &&sink,
                               type_list<Args...> /*arguments*/) {
  return std::forward<Sink>(sink)(resolver.template argument<Args>()...);
}
} // namespace detail

// Calls `sink` with the arguments of the constructor of `Type`: the one it
// declares, or the detected one.
template <typename Type, typename Resolver, typename Sink>
decltype(auto) invoke_constructor(Resolver &resolver, Sink &&sink) {
  if constexpr (declared_constructor<Type>::declared) {
    return detail::invoke_declared(
        resolver, std::forward<Sink>(sink),
        typename declared_constructor<Type>::arguments());
  } else {
    using constructor = detected_constructor<Type>;
    // The factory that calls it reports a constructor that is not detected
    // or generic.
    static_assert(constructor::kind == constructor_kind::concrete,
                  "a detected constructor is concrete");
    return detail::invoke_detected<Type>(
        resolver, std::forward<Sink>(sink),
        std::make_index_sequence<constructor::arity>{});
  }
}

} // namespace dingo::core
