//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/factory/callable.h>

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
// Invokes a callable with its arguments resolved from a container, inside the
// context that resolves them. The containers differ only in how they make that
// context; the dispatch is shared.
template <typename Signature, typename Context, typename Container,
          typename Callable>
decltype(auto) container_invoke(Context &context, Container &container,
                                construction_scope scope, Callable &&callable) {
  using callable_type = remove_cvref_t<Callable>;
  using dispatch_signature =
      callable_dispatch_signature_t<Signature, callable_type>;
  auto type_guard = context.template track_type<callable_type>();
  return callable_invoke<dispatch_signature>::construct(
      std::forward<Callable>(callable), scope, context, container);
}
} // namespace detail

template <typename T> struct invoke {
  template <typename Context, typename Container, typename Callable>
  static decltype(auto) construct(construction_scope scope, Context &ctx,
                                  Container &container, Callable &&callable) {
    return detail::callable_invoke<detail::callable_signature_t<T>>::construct(
        std::forward<Callable>(callable), scope, ctx, container);
  }
};

} // namespace dingo
