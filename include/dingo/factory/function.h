#pragma once

#include <dingo/config.h>

namespace dingo {
namespace detail {
template <typename T> struct function_impl;

template <typename T, typename... Args> struct function_impl<T (*)(Args...)> {
    template <typename Fn, typename Context> static T construct(Fn fn, Context& ctx) {
        return fn(ctx.template resolve<Args>()...);
    }
};

template <typename T, typename... Args> struct function_impl<T(Args...)> {
    template <typename Fn, typename Context> static T construct(Fn fn, Context& ctx) {
        return fn(ctx.template resolve<Args>()...);
    }
};

template <typename R, typename T, typename... Args> struct function_impl<R (T::*)(Args...) const> {
    template <typename Fn, typename Context> static R construct(Fn fn, Context& ctx) {
        return fn(ctx.template resolve<Args>()...);
    }
};
} // namespace detail

template <typename T, T fn> struct function_decl {
    template <typename Type, typename Context> static Type construct(Context& ctx) {
        return detail::function_impl<T>::construct(fn, ctx);
    }
};

template <auto fn> struct function : function_decl<decltype(fn), fn> {};

} // namespace dingo