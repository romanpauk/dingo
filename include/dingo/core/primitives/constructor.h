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
// The detection, with its backends and its signature mode, is in
// constructor_detection.h; this header holds what the engine uses.

#include <dingo/core/base/config.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/constructor_arity.h>
#include <dingo/core/primitives/constructor_arity_msvc.h>
#include <dingo/core/primitives/constructor_probe.h>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace dingo::core {

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

// The constructor a factory calls for `Type`, as the default backend detects
// it in the shape mode of constructor_detection.h.
template <typename Type> struct detected_constructor {
private:
  using search =
      detail::constructor_search_t<Type, default_constructor_detection,
                                   DINGO_CONSTRUCTOR_DETECTION_ARGS>;

public:
  static constexpr constructor_kind kind = detail::search_kind<search>;
  static constexpr std::size_t arity = detail::search_arity<search>;
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
  // Never viable. Without a second conversion template, GCC 13 binds an
  // rvalue reference parameter of a class with an explicit constructor through
  // `operator const Type &` instead of `operator Type`, and rejects it.
  template <typename Type> struct unnamed {};
  template <typename Type> operator unnamed<Type>() const = delete;

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
