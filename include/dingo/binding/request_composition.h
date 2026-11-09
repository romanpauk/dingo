//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
// A request of a wrapper its binding does not publish is composed: the
// value the wrapper holds is resolved, and the wrapper made of it. The value
// is one the request owns, moved into the wrapper: one made for the request,
// or a copy that shares what it was copied of, such as of a shared handle.
// What a scope keeps that shares nothing is not wrapped, as a copy of it is
// not the instance.

// The wrapper a request asks for, by value, by reference or by pointer.
template <typename Request>
using composed_type_t =
    std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<Request>>>;

// Whether a copy of a type shares what it holds: a shared handle does, and
// an optional of one.
template <typename Type, typename = void>
inline constexpr bool copy_shares_v = false;

template <typename Type>
inline constexpr bool copy_shares_v<
    Type,
    std::enable_if_t<type_traits<Type>::enabled && !std::is_pointer_v<Type> &&
                     std::is_copy_constructible_v<Type>>> =
    std::is_base_of_v<semantics::shared_owner, type_traits<Type>> ||
    (std::is_base_of_v<semantics::optional_value, type_traits<Type>> &&
     copy_shares_v<typename type_traits<Type>::value_type>);

// Whether a value composed for a request holds a copy that shares what it
// was copied of: it is such a copy, or a wrapper that cannot be copied, made
// of one.
template <typename Type, typename = void>
inline constexpr bool holds_sharing_copy_v = copy_shares_v<Type>;

template <typename Type>
inline constexpr bool holds_sharing_copy_v<
    Type,
    std::enable_if_t<type_traits<Type>::enabled && !std::is_pointer_v<Type> &&
                     !std::is_copy_constructible_v<Type>>> =
    holds_sharing_copy_v<typename type_traits<Type>::value_type>;

// The request of the value the wrapper of a request holds: the value itself
// when it is never a copy of what shares nothing, as one that shares what it
// holds, or one that cannot be copied, which is made or composed; otherwise
// what the request may move from, which is only what is made for it. What an
// rvalue request moves from is made for it.
template <typename Request,
          typename Value =
              typename type_traits<composed_type_t<Request>>::value_type>
using composed_inner_t =
    std::conditional_t<!std::is_rvalue_reference_v<Request> &&
                           (copy_shares_v<Value> ||
                            !std::is_copy_constructible_v<Value>),
                       Value, Value &&>;

// Whether a request is of a wrapper that can be made of the value it holds:
// a value that can exist, moved into a wrapper that can; a const value is
// not moved. The conversion is probed only then, as probing it instantiates
// both.
template <typename Request> constexpr bool composes() {
  using wrapper = composed_type_t<Request>;
  if constexpr (!type_traits<wrapper>::enabled || std::is_pointer_v<wrapper> ||
                is_array_like_type_v<wrapper>) {
    return false;
  } else {
    using value = typename type_traits<wrapper>::value_type;
    if constexpr (std::is_const_v<value> || !is_formable_v<value> ||
                  !is_wrapper_formable_v<wrapper>) {
      return false;
    } else {
      return is_type_conversion_available_v<wrapper, value &&, consume>;
    }
  }
}

template <typename Request>
inline constexpr bool composes_v = composes<Request>();

// Whether type_traits<Wrapper>::make makes the value of a wrapper of what
// makes the value, as for the wrappers of the standard library, so that a
// wrapper of a wrapper is made at once of what its innermost value is made of.
template <typename Wrapper> inline constexpr bool makes_nested_v = false;

template <typename T>
inline constexpr bool makes_nested_v<std::optional<T>> = true;

template <typename T>
inline constexpr bool makes_nested_v<std::shared_ptr<T>> = !std::is_array_v<T>;

template <typename T, typename Deleter>
inline constexpr bool makes_nested_v<std::unique_ptr<T, Deleter>> =
    !std::is_array_v<T>;

// Whether a reference or a pointer refers to a wrapper that holds a copy
// sharing what it was copied of, such as a std::shared_ptr<I> of a kept
// std::shared_ptr<T>, or a wrapper composed of one: a binding that keeps its
// instance keeps the wrapper with it, so it lives as long as the instance.
template <typename Request> constexpr bool refers_to_sharing() {
  if constexpr (std::is_lvalue_reference_v<Request> ||
                std::is_pointer_v<Request>) {
    using wrapper = composed_type_t<Request>;
    if constexpr (type_traits<wrapper>::enabled &&
                  !std::is_pointer_v<wrapper>) {
      return holds_sharing_copy_v<wrapper>;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <typename Request>
inline constexpr bool refers_to_sharing_v = refers_to_sharing<Request>();

} // namespace detail
} // namespace dingo
