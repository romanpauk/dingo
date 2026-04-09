//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/factory/callable.h>

#include <utility>

namespace dingo {

template <typename T> struct invoke {
    template <typename Context, typename Container, typename Callable>
    static decltype(auto) construct(Context& ctx, Container& container,
                                    Callable&& callable) {
        return detail::callable_invoke<detail::callable_signature_t<T>>::
            construct(std::forward<Callable>(callable), ctx, container);
    }
};

} // namespace dingo
