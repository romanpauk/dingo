//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

namespace dingo {
namespace detail {
template <typename T> struct function_impl;

template <typename T, typename... Args> struct function_impl<T (*)(Args...)> {
    template <typename Fn, typename Context, typename Container>
    static T construct(Fn fn, Context& ctx, Container& container) {
        return fn(ctx.template resolve<Args>(container)...);
    }

    template <typename Fn, typename Context, typename Container>
    static void construct(void* ptr, Fn fn, Context& ctx,
                          Container& container) {
        new (ptr) T{fn(ctx.template resolve<Args>(container)...)};
    }
};

template <typename T, typename... Args> struct function_impl<T(Args...)> {
    template <typename Fn, typename Context, typename Container>
    static T construct(Fn fn, Context& ctx, Container& container) {
        return fn(ctx.template resolve<Args>(container)...);
    }

    template <typename Fn, typename Context, typename Container>
    static void construct(void* ptr, Fn fn, Context& ctx,
                          Container& container) {
        new (ptr) T{fn(ctx.template resolve<Args>(container)...)};
    }
};

template <typename R, typename T, typename... Args>
struct function_impl<R (T::*)(Args...) const> {
    template <typename Fn, typename Context, typename Container>
    static R construct(Fn fn, Context& ctx, Container& container) {
        return fn(ctx.template resolve<Args>(container)...);
    }

    template <typename Fn, typename Context, typename Container>
    static void construct(void* ptr, Fn fn, Context& ctx,
                          Container& container) {
        new (ptr) R{fn(ctx.template resolve<Args>(container)...)};
    }
};
} // namespace detail

template <typename T, T fn> struct function_decl {
    template <typename Type, typename Context, typename Container>
    static Type construct(Context& ctx, Container& container) {
        return detail::function_impl<T>::construct(fn, ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        new (ptr) Type{detail::function_impl<T>::construct(fn, ctx, container)};
    }
};

template <auto fn> struct function : function_decl<decltype(fn), fn> {};

} // namespace dingo