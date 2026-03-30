//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/factory/class_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/factory/constructor_detection.h>
#include <dingo/type/type_list.h>

namespace dingo {

template <typename...> struct constructor;

template <typename T, typename... Args> struct constructor<T(Args...)> {
    using arguments = type_list<Args...>;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool valid =
        detail::is_list_initializable_v<T, Args...> ||
        detail::is_direct_initializable_v<T, Args...>;

    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return detail::construction_dispatch<Type, T>::construct(
            ctx.template resolve<Args>(container)...);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        detail::construction_dispatch<Type, T>::construct(
            ptr, ctx.template resolve<Args>(container)...);
    }
};

template <typename T, typename... Args>
struct constructor<T, Args...> : constructor<T(Args...)> {};

} // namespace dingo
