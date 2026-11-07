//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>

#include <functional>
#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Signature> struct callable_invoke;

template <typename R, typename... Args> struct callable_invoke<R(Args...)> {
  // A callable is called directly, so that a value its parameter takes
  // initializes it in place: a type that can be neither copied nor moved can
  // be passed by value. std::invoke would take it as a reference and have to
  // move it. A member pointer is invoked.
  template <typename Fn, typename Context, typename Container, typename Scope>
  static decltype(auto) construct(Fn &&fn, Scope scope, Context &ctx,
                                  Container &container) {
    (void)scope;
    if constexpr (std::is_member_pointer_v<std::decay_t<Fn>>) {
      return std::invoke(
          std::forward<Fn>(fn),
          ctx.template resolve<Args>(detail::dependency_scope<Args>(scope),
                                     container)...);
    } else {
      return std::forward<Fn>(fn)(ctx.template resolve<Args>(
          detail::dependency_scope<Args>(scope), container)...);
    }
  }

  // An object constructed in place, as a cyclical scope constructs it, is
  // initialized with what the callable returns, so the callable returns the
  // object: a handle it returns owns an object of its own.
  template <typename Type, typename Fn, typename Context, typename Container,
            typename Scope>
  static auto construct(void *ptr, Fn &&fn, Scope scope, Context &ctx,
                        Container &container) {
    using result_type =
        decltype(construct(std::forward<Fn>(fn), scope, ctx, container));
    if constexpr (!std::is_constructible_v<normalized_type_t<Type>,
                                           result_type>) {
      static_assert(
          std::is_constructible_v<normalized_type_t<Type>, result_type>,
          "a factory of an object constructed in place, as a cyclical "
          "scope constructs it, returns the object, not a handle of it");
    } else {
      new (ptr) normalized_type_t<Type>(
          construct(std::forward<Fn>(fn), scope, ctx, container));
    }
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

#define DINGO_CALLABLE_SIGNATURE_VARIANTS(APPLY)                               \
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

// NOLINTBEGIN(bugprone-macro-parentheses)

#define DINGO_DEFINE_CALLABLE_SIGNATURE(cv_qualifier, ref_qualifier,           \
                                        noexcept_qualifier)                    \
  template <typename R, typename... Args>                                      \
  struct callable_signature<                                                   \
      R(Args...) cv_qualifier ref_qualifier noexcept_qualifier, void>          \
      : callable_signature<R(Args...) noexcept_qualifier> {};

DINGO_CALLABLE_SIGNATURE_VARIANTS(DINGO_DEFINE_CALLABLE_SIGNATURE)

#undef DINGO_DEFINE_CALLABLE_SIGNATURE

template <typename T>
struct callable_signature<T *, std::enable_if_t<std::is_function_v<T>>>
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
    : callable_signature<R(Class &, Args...)> {};

template <typename Class, typename R, typename... Args>
struct callable_signature<R (Class::*)(Args...) noexcept, void>
    : callable_signature<R(Class &, Args...) noexcept> {};

template <typename T> struct callable_operator_signature;

template <typename Class, typename R, typename... Args>
struct callable_operator_signature<R (Class::*)(Args...)>
    : callable_signature<R(Args...)> {};

template <typename Class, typename R, typename... Args>
struct callable_operator_signature<R (Class::*)(Args...) noexcept>
    : callable_signature<R(Args...) noexcept> {};

#define DINGO_MEMBER_CALLABLE_SIGNATURE_VARIANTS(APPLY)                        \
  APPLY(, &, Class &, )                                                        \
  APPLY(, &, Class &, noexcept)                                                \
  APPLY(, &&, Class &&, )                                                      \
  APPLY(, &&, Class &&, noexcept)                                              \
  APPLY(const, , const Class &, )                                              \
  APPLY(const, , const Class &, noexcept)                                      \
  APPLY(const, &, const Class &, )                                             \
  APPLY(const, &, const Class &, noexcept)                                     \
  APPLY(const, &&, const Class &&, )                                           \
  APPLY(const, &&, const Class &&, noexcept)                                   \
  APPLY(volatile, , volatile Class &, )                                        \
  APPLY(volatile, , volatile Class &, noexcept)                                \
  APPLY(volatile, &, volatile Class &, )                                       \
  APPLY(volatile, &, volatile Class &, noexcept)                               \
  APPLY(volatile, &&, volatile Class &&, )                                     \
  APPLY(volatile, &&, volatile Class &&, noexcept)                             \
  APPLY(const volatile, , const volatile Class &, )                            \
  APPLY(const volatile, , const volatile Class &, noexcept)                    \
  APPLY(const volatile, &, const volatile Class &, )                           \
  APPLY(const volatile, &, const volatile Class &, noexcept)                   \
  APPLY(const volatile, &&, const volatile Class &&, )                         \
  APPLY(const volatile, &&, const volatile Class &&, noexcept)

#define DINGO_DEFINE_MEMBER_CALLABLE_SIGNATURE(                                \
    cv_qualifier, ref_qualifier, object_type, noexcept_qualifier)              \
  template <typename Class, typename R, typename... Args>                      \
  struct callable_signature<R (Class::*)(Args...)                              \
                                cv_qualifier ref_qualifier noexcept_qualifier, \
                            void>                                              \
      : callable_signature<R(object_type, Args...) noexcept_qualifier> {};

DINGO_MEMBER_CALLABLE_SIGNATURE_VARIANTS(DINGO_DEFINE_MEMBER_CALLABLE_SIGNATURE)

#undef DINGO_DEFINE_MEMBER_CALLABLE_SIGNATURE
#undef DINGO_MEMBER_CALLABLE_SIGNATURE_VARIANTS

#define DINGO_DEFINE_CALLABLE_OPERATOR_SIGNATURE(cv_qualifier, ref_qualifier,  \
                                                 noexcept_qualifier)           \
  template <typename Class, typename R, typename... Args>                      \
  struct callable_operator_signature<R (Class::*)(                             \
      Args...) cv_qualifier ref_qualifier noexcept_qualifier>                  \
      : callable_signature<R(Args...) noexcept_qualifier> {};

DINGO_CALLABLE_SIGNATURE_VARIANTS(DINGO_DEFINE_CALLABLE_OPERATOR_SIGNATURE)

#undef DINGO_DEFINE_CALLABLE_OPERATOR_SIGNATURE
#undef DINGO_CALLABLE_SIGNATURE_VARIANTS

// NOLINTEND(bugprone-macro-parentheses)

template <typename T>
struct callable_signature<T,
                          std::void_t<decltype(&remove_cvref_t<T>::operator())>>
    : callable_operator_signature<decltype(&remove_cvref_t<T>::operator())> {};

template <typename T>
using callable_signature_t =
    typename callable_signature<remove_cvref_t<T>>::type;

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

template <typename Signature, typename T> struct callable_factory {
  explicit callable_factory(T fn) : fn_(std::move(fn)) {}

  template <typename Type, typename Context, typename Container, typename Scope>
  auto construct(Scope scope, Context &ctx, Container &container) {
    return callable_invoke<Signature>::construct(fn_, scope, ctx, container);
  }

  template <typename Type, typename Context, typename Container, typename Scope>
  auto construct(void *ptr, Scope scope, Context &ctx, Container &container) {
    callable_invoke<Signature>::template construct<Type>(ptr, fn_, scope, ctx,
                                                         container);
  }

private:
  T fn_;
};
} // namespace detail

template <typename Signature = void, typename T> auto callable(T &&fn) {
  using fn_type = detail::remove_cvref_t<T>;
  using dispatch_signature =
      detail::callable_dispatch_signature_t<Signature, fn_type>;

  return detail::callable_factory<dispatch_signature, fn_type>(
      std::forward<T>(fn));
}

} // namespace dingo
