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

template <typename Type, typename = void>
struct is_auto_constructible : std::false_type {};

template <typename Type>
struct is_auto_constructible<
    Type, std::enable_if_t<core::detail::is_complete<Type>::value>>
    : std::bool_constant<std::is_aggregate_v<Type>> {};

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

} // namespace dingo::core
