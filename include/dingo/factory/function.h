//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/factory/callable.h>

namespace dingo {

template <typename T, T fn> struct function_decl {
    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return detail::callable_invoke<
            detail::callable_signature_t<T>>::construct(fn, ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        detail::callable_invoke<detail::callable_signature_t<T>>::
            template construct<Type>(ptr, fn, ctx, container);
    }
};

template <auto fn> struct function : function_decl<decltype(fn), fn> {};

} // namespace dingo
