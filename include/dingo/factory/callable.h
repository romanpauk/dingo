//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/factory/function.h>

namespace dingo {

template <typename T> struct callable {
    callable(T fn) : fn_(fn) {}

    template <typename Type, typename Context, typename Container>
    auto construct(Context& ctx, Container& container) {
        return detail::function_impl<decltype(&T::operator())>::construct(
            fn_, ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    void construct(void* ptr, Context& ctx, Container& container) {
        // TODO
        new (ptr) normalized_type_t<Type>(
            detail::function_impl<decltype(&T::operator())>::construct(
                fn_, ctx, container));
    }

  private:
    T fn_;
};

namespace detail {
template <typename Signature, typename Callable>
struct callable_invocation_ir;

template <typename R, typename C, typename... Args, typename Callable>
struct callable_invocation_ir<R (C::*)(Args...) const, Callable> {
    using type = ir::function_invocation<R, type_list<Args...>,
                                         callable<Callable>>;
};

template <typename Callable>
struct factory_invocation_ir<callable<Callable>>
    : callable_invocation_ir<decltype(&Callable::operator()), Callable> {};
} // namespace detail

} // namespace dingo
