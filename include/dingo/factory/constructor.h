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
#include <dingo/resolution/ir.h>
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
        using invocation = detail::factory_invocation_ir_t<constructor<T(Args...)>>;
        return ctx.template invoke_constructor<invocation>([&] {
            return detail::construction_dispatch<Type, T>::construct(
                ctx.template resolve<Args>(container)...);
        });
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        using invocation = detail::factory_invocation_ir_t<constructor<T(Args...)>>;
        ctx.template invoke_constructor<invocation>([&] {
            detail::construction_dispatch<Type, T>::construct(
                ptr, ctx.template resolve<Args>(container)...);
        });
    }
};

template <typename T, typename... Args>
struct constructor<T, Args...> : constructor<T(Args...)> {};

namespace detail {
template <typename T, typename... Args>
struct factory_invocation_ir<constructor<T(Args...)>> {
    using type = ir::constructor_invocation<T, type_list<Args...>>;
};

template <typename T, typename... Args>
struct factory_invocation_ir<constructor<T, Args...>>
    : factory_invocation_ir<constructor<T(Args...)>> {};
} // namespace detail

} // namespace dingo
