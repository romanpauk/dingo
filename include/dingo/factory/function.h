//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

#include <dingo/type/normalized_type.h>

#include <functional>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Signature> struct callable_invoke;

template <typename R, typename... Args> struct callable_invoke<R(Args...)> {
    template <typename Fn, typename Context, typename Container>
    static decltype(auto) construct(Fn&& fn, Context& ctx,
                                    Container& container) {
        return std::invoke(std::forward<Fn>(fn),
                           ctx.template resolve<Args>(container)...);
    }

    template <typename Type, typename Fn, typename Context, typename Container>
    static void construct(void* ptr, Fn&& fn, Context& ctx,
                          Container& container) {
        new (ptr) normalized_type_t<Type>(construct(std::forward<Fn>(fn), ctx,
                                                    container));
    }
};

template <typename R, typename... Args>
struct callable_invoke<R(Args...) noexcept> : callable_invoke<R(Args...)> {};

template <typename T, typename = void> struct callable_signature;

template <typename R, typename... Args>
struct callable_signature<R(Args...), void> {
    using type = R(Args...);
};

template <typename R, typename... Args>
struct callable_signature<R(Args...) noexcept, void> {
    using type = R(Args...) noexcept;
};

#define DINGO_CALLABLE_SIGNATURE_VARIANTS(APPLY)                                 \
    APPLY(, &, )                                                                 \
    APPLY(, &, noexcept)                                                         \
    APPLY(, &&, )                                                                \
    APPLY(, &&, noexcept)                                                        \
    APPLY(const, , )                                                             \
    APPLY(const, , noexcept)                                                     \
    APPLY(const, &, )                                                            \
    APPLY(const, &, noexcept)                                                    \
    APPLY(const, &&, )                                                           \
    APPLY(const, &&, noexcept)                                                   \
    APPLY(volatile, , )                                                          \
    APPLY(volatile, , noexcept)                                                  \
    APPLY(volatile, &, )                                                         \
    APPLY(volatile, &, noexcept)                                                 \
    APPLY(volatile, &&, )                                                        \
    APPLY(volatile, &&, noexcept)                                                \
    APPLY(const volatile, , )                                                    \
    APPLY(const volatile, , noexcept)                                            \
    APPLY(const volatile, &, )                                                   \
    APPLY(const volatile, &, noexcept)                                           \
    APPLY(const volatile, &&, )                                                  \
    APPLY(const volatile, &&, noexcept)

#define DINGO_DEFINE_CALLABLE_SIGNATURE(cv_qualifier, ref_qualifier,             \
                                        noexcept_qualifier)                      \
    template <typename R, typename... Args>                                      \
    struct callable_signature<                                                   \
        R(Args...) cv_qualifier ref_qualifier noexcept_qualifier, void>          \
        : callable_signature<R(Args...) noexcept_qualifier> {};

DINGO_CALLABLE_SIGNATURE_VARIANTS(DINGO_DEFINE_CALLABLE_SIGNATURE)

#undef DINGO_DEFINE_CALLABLE_SIGNATURE

template <typename T>
struct callable_signature<T*, std::enable_if_t<std::is_function_v<T>>>
    : callable_signature<T> {};

template <typename Signature>
struct callable_signature<std::function<Signature>, void>
    : callable_signature<Signature> {};

#if defined(__cpp_lib_move_only_function)
template <typename Signature>
struct callable_signature<std::move_only_function<Signature>, void>
    : callable_signature<Signature> {};
#endif

#if defined(__cpp_lib_copyable_function)
template <typename Signature>
struct callable_signature<std::copyable_function<Signature>, void>
    : callable_signature<Signature> {};
#endif

template <typename Class, typename R, typename... Args>
struct callable_signature<R (Class::*)(Args...), void>
    : callable_signature<R(Args...)> {};

template <typename Class, typename R, typename... Args>
struct callable_signature<R (Class::*)(Args...) noexcept, void>
    : callable_signature<R(Args...) noexcept> {};

#define DINGO_DEFINE_MEMBER_CALLABLE_SIGNATURE(cv_qualifier, ref_qualifier,      \
                                               noexcept_qualifier)               \
    template <typename Class, typename R, typename... Args>                      \
    struct callable_signature<                                                   \
        R (Class::*)(Args...) cv_qualifier ref_qualifier noexcept_qualifier,     \
        void> : callable_signature<R(Args...) noexcept_qualifier> {};

DINGO_CALLABLE_SIGNATURE_VARIANTS(DINGO_DEFINE_MEMBER_CALLABLE_SIGNATURE)

#undef DINGO_DEFINE_MEMBER_CALLABLE_SIGNATURE
#undef DINGO_CALLABLE_SIGNATURE_VARIANTS

template <typename T>
struct callable_signature<
    T, std::void_t<decltype(&remove_cvref_t<T>::operator())>>
    : callable_signature<decltype(&remove_cvref_t<T>::operator())> {};

template <typename T>
using callable_signature_t = typename callable_signature<remove_cvref_t<T>>::type;

template <typename Signature, typename Callable>
struct callable_dispatch_signature {
    using type = Signature;
};

template <typename Callable>
struct callable_dispatch_signature<void, Callable> {
    using type = callable_signature_t<Callable>;
};

template <typename Signature, typename Callable>
using callable_dispatch_signature_t =
    typename callable_dispatch_signature<Signature, Callable>::type;

template <typename T> struct function_impl : callable_invoke<callable_signature_t<T>> {};
} // namespace detail

template <typename T, T fn> struct function_decl {
    template <typename Type, typename Context, typename Container>
    static auto construct(Context& ctx, Container& container) {
        return detail::function_impl<T>::construct(fn, ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    static void construct(void* ptr, Context& ctx, Container& container) {
        detail::function_impl<T>::template construct<Type>(ptr, fn, ctx,
                                                           container);
    }
};

template <auto fn> struct function : function_decl<decltype(fn), fn> {};

} // namespace dingo
