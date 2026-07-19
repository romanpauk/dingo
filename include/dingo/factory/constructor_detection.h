//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Public constructor detection interface.
//
// constructor_detection<T> describes the constructor Dingo selects for T and
// invokes that same constructor during injection. An explicit constructor
// declaration takes precedence over automatic detection. Otherwise detection
// searches from constructor_detection_traits<T>::max_arity down to zero.
//
// The result exposes the selected constructor kind and arity. Detection
// strategies that recover concrete parameters also expose their argument type
// list. Both construct overloads use the constructor selected by the result.

#include <dingo/core/config.h>

#include <dingo/core/construction_scope.h>
#include <dingo/core/key.h>
#include <dingo/factory/constructor_traits.h>
#include <dingo/factory/constructor_typedef.h>
#include <dingo/registration/annotated.h>
#include <dingo/type/complete_type.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

#include <type_traits>
#include <utility>

namespace dingo {

// Controls the largest constructor arity considered for T. Specialize this
// trait when a type needs a limit different from the library default.
template <typename T> struct constructor_detection_traits {
  static constexpr size_t max_arity = DINGO_CONSTRUCTOR_DETECTION_ARGS;
};

template <typename...> struct constructor;

} // namespace dingo

#include <dingo/factory/detail/constructor_detection.hpp>

#if defined(_MSC_VER)
#include <dingo/factory/detail/constructor_detection_msvc.hpp>
#endif

namespace dingo {

#if defined(_MSC_VER)
template <typename T, typename DetectionMode = detail::constructor_shape>
using constructor_detection = std::conditional_t<
    has_constructor_typedef_v<T>, constructor_typedef<T>,
    detail::constructor_detection_msvc<
        T, DetectionMode, detail::list_initialization,
        constructor_detection_traits<normalized_type_t<T>>::max_arity>>;
#else
template <typename T, typename DetectionMode = detail::constructor_shape>
using constructor_detection = std::conditional_t<
    has_constructor_typedef_v<T>, constructor_typedef<T>,
    detail::constructor_detection<
        T, DetectionMode, detail::list_initialization,
        constructor_detection_traits<normalized_type_t<T>>::max_arity>>;
#endif

} // namespace dingo
