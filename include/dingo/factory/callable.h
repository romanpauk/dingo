//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/factory/callable.h of the next implementation: the callable factory,
// which calls a callable object passed at registration whose result is the
// stored object.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/projection/construct.h>

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Signature, typename Callable> struct callable_arguments {
  using type = core::callable_arguments_t<std::decay_t<Signature>>;
};

template <typename Callable> struct callable_arguments<void, Callable> {
  using type = core::callable_arguments_t<Callable>;
};
} // namespace detail

// A callable factory; `Signature`, when given, names the argument types of a
// callable whose call operator is overloaded or a template.
template <typename Signature = void, typename Callable>
auto callable(Callable &&function) {
  using callable_type = std::decay_t<Callable>;
  return core::callable_factory<
      typename detail::callable_arguments<Signature, callable_type>::type,
      callable_type>(std::forward<Callable>(function));
}

} // namespace dingo
