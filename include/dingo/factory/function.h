//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/factory/callable.h>

namespace dingo {

// A factory that calls fn with the dependencies its parameters name.
template <auto fn> struct function {
  using signature = detail::callable_signature_t<decltype(fn)>;

  template <typename Type, typename Context, typename Container>
  static auto construct(construction_scope scope, Context &ctx,
                        Container &container) {
    return detail::callable_invoke<signature>::construct(fn, scope, ctx,
                                                         container);
  }

  template <typename Type, typename Context, typename Container>
  static auto construct(void *ptr, construction_scope scope, Context &ctx,
                        Container &container) {
    detail::callable_invoke<signature>::template construct<Type>(
        ptr, fn, scope, ctx, container);
  }
};

} // namespace dingo
