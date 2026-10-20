//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The public extension point that lets a container construct an unregistered
// type a request names when no binding is named: a complete aggregate by
// default, and any type whose specialization opts in.

#include <dingo/core/primitives/constructor.h>

#include <type_traits>

namespace dingo {

namespace detail {
template <typename Type, typename = void>
struct default_auto_constructible : std::false_type {};

template <typename Type>
struct default_auto_constructible<
    Type, std::enable_if_t<core::detail::is_complete<Type>::value>>
    : std::bool_constant<std::is_aggregate_v<Type>> {};
} // namespace detail

// Whether an unregistered type is auto-constructed: a complete aggregate by
// default. It takes the type alone, so a specialization for a template of
// types is partial without competing with the default.
template <typename Type>
struct is_auto_constructible : detail::default_auto_constructible<Type> {};

} // namespace dingo

namespace dingo::core {

// Whether a request of `Request` may construct its type: a value or a const
// lvalue reference of an auto-constructible leaf whose constructor is
// detected.
template <typename Request, typename = void>
struct auto_constructs : std::false_type {};

template <typename Request>
struct auto_constructs<
    Request,
    std::enable_if_t<(!std::is_reference_v<Request> ||
                      (std::is_lvalue_reference_v<Request> &&
                       std::is_const_v<std::remove_reference_t<Request>>)) &&
                     !std::is_pointer_v<std::remove_reference_t<Request>> &&
                     !std::is_array_v<std::remove_reference_t<Request>> &&
                     ::dingo::is_auto_constructible<std::remove_cv_t<
                         std::remove_reference_t<Request>>>::value>>
    : std::bool_constant<detected_constructor<std::remove_cv_t<
                             std::remove_reference_t<Request>>>::kind ==
                         constructor_kind::concrete> {};

// Whether a request auto-constructs by the arguments `Resolver` resolves its
// dependencies with: the detected constructor accepts them as well as the
// probes it was detected by, which convert unambiguously.
template <typename Request, typename Resolver>
constexpr bool auto_constructs_by() {
  if constexpr (!auto_constructs<Request>::value) {
    return false;
  } else {
    using value_type = std::remove_cv_t<std::remove_reference_t<Request>>;
    return detail::constructible_with<
        value_type, constructor_argument<value_type, Resolver>>(
        std::make_index_sequence<detected_constructor<value_type>::arity>{});
  }
}

} // namespace dingo::core
