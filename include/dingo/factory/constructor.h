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

template <typename T> struct constructor<T> : constructor_detection<T> {};

template <typename T, typename... Args> struct constructor<T(Args...)> {
  using arguments = type_list<Args...>;
  static constexpr size_t arity = sizeof...(Args);
  static constexpr bool valid = detail::is_list_initializable_v<T, Args...> ||
                                detail::is_direct_initializable_v<T, Args...>;

  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    (void)scope;
    return detail::construction_dispatch<Type, T>::construct(
        ctx.template resolve<Args>(detail::dependency_scope<Args>(scope),
                                   container)...);
  }

  template <typename Type, typename Context, typename Container>
  static void construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    (void)scope;
    detail::construction_dispatch<Type, T>::construct(
        ptr, ctx.template resolve<Args>(detail::dependency_scope<Args>(scope),
                                        container)...);
  }
};

} // namespace dingo
