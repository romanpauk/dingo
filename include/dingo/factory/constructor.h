//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/factory/constructor_detection.h>
#include <dingo/factory/constructor_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

namespace dingo {

template <typename...> struct constructor;

namespace detail {

template <typename Dependency, typename Context, typename Container>
class constructor_dependency {
public:
  constructor_dependency(construction_scope scope, Context &context,
                         Container &container)
      : scope_(scope), context_(context), container_(container) {}

  operator Dependency() {
    return context_.template resolve<Dependency>(
        dependency_scope<Dependency>(scope_), container_);
  }

private:
  construction_scope scope_;
  Context &context_;
  Container &container_;
};

template <typename Dependency, typename Context, typename Container>
decltype(auto) resolve_constructor_dependency(construction_scope scope,
                                              Context &context,
                                              Container &container) {
  if constexpr (std::is_reference_v<Dependency> ||
                std::is_move_constructible_v<Dependency>) {
    return context.template resolve<Dependency>(
        dependency_scope<Dependency>(scope), container);
  } else {
    // Keep a copy-only prvalue in the final parameter initialization, where
    // C++17 guaranteed elision avoids selecting a deleted move constructor.
    return constructor_dependency<Dependency, Context, Container>(
        scope, context, container);
  }
}

} // namespace detail

template <typename T, typename... Args> struct constructor<T(Args...)> {
  using arguments = type_list<Args...>;
  static constexpr size_t arity = sizeof...(Args);
  static constexpr bool valid = detail::is_list_initializable_v<T, Args...> ||
                                detail::is_direct_initializable_v<T, Args...>;
  static constexpr detail::constructor_kind kind =
      valid ? detail::constructor_kind::concrete
            : detail::constructor_kind::invalid;

  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    (void)scope;
    return detail::construction_dispatch<Type, T>::construct(
        detail::resolve_constructor_dependency<Args>(scope, ctx, container)...);
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    (void)scope;
    detail::construction_dispatch<Type, T>::construct(
        ptr,
        detail::resolve_constructor_dependency<Args>(scope, ctx, container)...);
  }
};

// Unqualified scalar object types cannot declare injectable constructor
// parameters. Keep cv-qualified scalars on the detection path so the shortcut
// does not form deprecated function types such as volatile int().
namespace detail {
template <typename T>
struct scalar_constructor
    : std::conditional_t<std::is_same_v<T, std::remove_cv_t<T>>,
                         ::dingo::constructor<std::remove_cv_t<T>()>,
                         ::dingo::constructor_detection<T>> {};
} // namespace detail

template <typename T>
struct constructor<T>
    : std::conditional_t<std::is_scalar_v<T>, detail::scalar_constructor<T>,
                         constructor_detection<T>> {};

} // namespace dingo
