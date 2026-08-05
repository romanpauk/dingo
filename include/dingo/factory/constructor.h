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

template <typename T, bool Candidate = !has_constructor_typedef_v<T> &&
                                       std::is_empty_v<T> &&
                                       std::is_aggregate_v<T>>
struct zero_argument_aggregate_impl : std::false_type {};

template <typename T>
struct zero_argument_aggregate_impl<T, true>
    : std::bool_constant<is_list_initializable_v<T> &&
                         !is_list_initializable_v<
                             T, constructor_argument<T, constructor_shape>>> {};

template <typename T, bool Complete = is_complete<T>::value>
struct is_zero_argument_aggregate : std::false_type {};

template <typename T>
struct is_zero_argument_aggregate<T, true> : zero_argument_aggregate_impl<T> {};

template <typename T>
inline constexpr bool is_zero_argument_aggregate_v =
    is_zero_argument_aggregate<T>::value;

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

// Unqualified scalar types and empty aggregates without an initializable
// element cannot declare injectable constructor parameters. Keep cv-qualified
// types on the detection path so the shortcut does not form deprecated function
// types such as volatile int().
namespace detail {
template <typename T>
struct zero_argument_constructor
    : std::conditional_t<std::is_same_v<T, std::remove_cv_t<T>>,
                         ::dingo::constructor<std::remove_cv_t<T>()>,
                         ::dingo::constructor_detection<T>> {};
} // namespace detail

template <typename T>
struct constructor<T>
    : std::conditional_t<
          std::is_scalar_v<T> || detail::is_zero_argument_aggregate_v<T>,
          detail::zero_argument_constructor<T>, constructor_detection<T>> {};

} // namespace dingo
