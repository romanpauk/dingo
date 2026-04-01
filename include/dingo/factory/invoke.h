//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/factory/constructor.h>

#include <functional>

namespace dingo {
namespace detail {
template <typename... Args, typename Context, typename Container,
          typename Callable>
decltype(auto) invoke(Context& ctx, Container& container,
                                         Callable&& callable) {
    return std::forward<Callable>(callable)(
        ((void)sizeof(Args),
         constructor_argument_impl<void, Context, Container, automatic>(
             ctx, container))...);
}
} // namespace detail

    template< typename T > struct invoke: invoke< decltype(&T::operator()) > {};

    template< typename T, typename R, typename... Args > struct invoke< R(T::*)(Args...) >: invoke< R(T::*)(Args...) const > {};

    template< typename T, typename R, typename... Args > struct invoke< R(T::*)(Args...) const > {
        template< typename Context, typename Container, typename Callable > static R construct(Context& ctx, Container& container, Callable&& callable) {
            return detail::invoke<Args...>(ctx, container,
                                                              std::forward<Callable>(callable));
        }
    };

    template< typename R, typename... Args > struct invoke< std::function<R(Args...)> > {
        template< typename Context, typename Container, typename Callable > static R construct(Context& ctx, Container& container, Callable&& callable) {
            return detail::invoke<Args...>(ctx, container,
                                                              std::forward<Callable>(callable));
        }
    };

    template< typename R, typename... Args > struct invoke< R(*)(Args...) > {
        template< typename Context, typename Container, typename Callable > static R construct(Context& ctx, Container& container, Callable&& callable) {
            return detail::invoke<Args...>(ctx, container,
                                                              std::forward<Callable>(callable));
        }
    };

    template< typename R, typename... Args > struct invoke< R(Args...) > {
        template< typename Context, typename Container, typename Callable > static R construct(Context& ctx, Container& container, Callable&& callable) {
            return detail::invoke<Args...>(ctx, container,
                                                              std::forward<Callable>(callable));
        }
    };

}
