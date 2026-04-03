//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>
#include <dingo/factory/function.h>

#include <utility>

namespace dingo {

namespace detail {
template <typename Signature, typename T> struct callable_factory {
    explicit callable_factory(T fn) : fn_(std::move(fn)) {}

    template <typename Type, typename Context, typename Container>
    auto construct(Context& ctx, Container& container) {
        return callable_invoke<Signature>::construct(fn_, ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    void construct(void* ptr, Context& ctx, Container& container) {
        callable_invoke<Signature>::template construct<Type>(ptr, fn_, ctx,
                                                             container);
    }

  private:
    T fn_;
};
} // namespace detail

template <typename Signature = void, typename T>
auto callable(T&& fn) {
    using fn_type = detail::remove_cvref_t<T>;
    using dispatch_signature =
        detail::callable_dispatch_signature_t<Signature, fn_type>;

    return detail::callable_factory<dispatch_signature, fn_type>(
        std::forward<T>(fn));
}

} // namespace dingo
