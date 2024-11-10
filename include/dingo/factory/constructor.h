//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/class_traits.h>
#include <dingo/decay.h>
#include <dingo/factory/constructor_detection.h>

namespace dingo {

template <typename...> struct constructor;

template <typename T, typename... Args> struct constructor<T(Args...)> {
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid =
        detail::is_list_initializable_v<T, Args...> ||
        detail::is_direct_initializable_v<T, Args...>;

    template <typename Type, typename Context, typename Container>
    static Type construct(Context& ctx, Container& container) {
        return class_traits<Type>::construct(
            ctx.template resolve<Args>(container)...);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        class_traits<Type>::construct(ptr,
                                      ctx.template resolve<Args>(container)...);
    }
};

template <typename T, typename... Args>
struct constructor<T, Args...> : constructor<T(Args...)> {};

} // namespace dingo
