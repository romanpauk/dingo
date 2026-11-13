//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/conversion/paths.h>
#include <dingo/type/type_traits.h>

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

// Whether a type is a wrapper, which is asked of what it holds: a type only
// declared is not, and nothing else is asked of it.
template <typename Type>
inline constexpr bool is_object_wrapper_v =
    type_traits<Type>::enabled && !std::is_pointer_v<Type>;

// Whether a copy of a type shares what it holds: a shared handle does, and
// an optional of one.
template <typename Type> constexpr bool copy_shares() {
  if constexpr (is_object_wrapper_v<Type>) {
    using traits = type_traits<Type>;
    if constexpr (std::is_base_of_v<semantics::shared_owner, traits>) {
      return std::is_copy_constructible_v<Type>;
    } else if constexpr (std::is_base_of_v<semantics::optional_value, traits>) {
      return std::is_copy_constructible_v<Type> &&
             copy_shares<typename traits::value_type>();
    } else {
      return false;
    }
  } else {
    return false;
  }
}

template <typename Type>
inline constexpr bool copy_shares_v = copy_shares<Type>();

// Whether a value composed for a request holds a copy that shares what it
// was copied of: it is such a copy, or a wrapper that cannot be copied, made
// of one.
template <typename Type> constexpr bool holds_sharing_copy() {
  if constexpr (!is_object_wrapper_v<Type>) {
    return false;
  } else if constexpr (!std::is_copy_constructible_v<Type>) {
    return holds_sharing_copy<typename type_traits<Type>::value_type>();
  } else {
    return copy_shares<Type>();
  }
}

template <typename Type>
inline constexpr bool holds_sharing_copy_v = holds_sharing_copy<Type>();

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
// not moved, and a value of a type only declared where it is requested
// cannot be moved there. The conversion is probed only then, as probing it
// instantiates both.
template <typename Request> constexpr bool composes() {
  using wrapper = composed_type_t<Request>;
  if constexpr (type_traits<wrapper>::enabled && !std::is_pointer_v<wrapper> &&
                !is_array_like_type_v<wrapper>) {
    using value = typename type_traits<wrapper>::value_type;
    if constexpr (!std::is_const_v<value> && is_complete_v<value>) {
      if constexpr (is_formable_v<value> && is_wrapper_formable_v<wrapper>) {
        return is_type_conversion_available_v<wrapper, value &&, consume>;
      }
    }
  }
  return false;
}

template <typename Request>
inline constexpr bool composes_v = composes<Request>();

// Whether type_traits<Wrapper>::emplace emplaces an element that is a wrapper
// of the arguments of the element's own element, as the wrapper declares with
// emplaces_element.
template <typename Wrapper, typename = void>
inline constexpr bool emplaces_element_v = false;

template <typename Wrapper>
inline constexpr bool emplaces_element_v<
    Wrapper, std::void_t<decltype(type_traits<Wrapper>::emplaces_element)>> =
    type_traits<Wrapper>::emplaces_element;

// Whether a wrapper is emplaced at once of Value, its innermost value, down
// layers that each emplace their element.
template <typename Wrapper, typename Value> constexpr bool emplaces_from() {
  if constexpr (!emplaces_element_v<Wrapper>) {
    return false;
  } else {
    using value_type = typename type_traits<Wrapper>::value_type;
    if constexpr (std::is_same_v<std::remove_cv_t<value_type>, Value>) {
      return true;
    } else {
      return emplaces_from<std::remove_cv_t<value_type>, Value>();
    }
  }
}

template <typename Wrapper, typename Value>
inline constexpr bool emplaces_from_v = emplaces_from<Wrapper, Value>();

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
