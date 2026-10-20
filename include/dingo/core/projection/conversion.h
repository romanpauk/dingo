//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The public extension point that converts a wrapper to the wrapper of an
// interface of its element: a specialization converts a Source object to a
// Target object, and declares the access it needs, to borrow the source or
// consume it. Without one, a wrapper converts by its converting constructor.

#include <type_traits>
#include <utility>

namespace dingo {

// The access of a conversion that reads its source without consuming it.
struct borrow {};
// The access of a conversion that consumes its source.
struct consume {};

template <typename Target, typename Source, typename = void>
struct type_conversion_traits {};

} // namespace dingo

namespace dingo::core {

namespace detail {
template <typename Target, typename Source, typename From, typename = void>
struct converts_from : std::false_type {};

template <typename Target, typename Source, typename From>
struct converts_from<
    Target, Source, From,
    std::void_t<decltype(::dingo::type_conversion_traits<
                         Target, Source>::convert(std::declval<From>()))>>
    : std::true_type {};
} // namespace detail

// Whether a specialization declares a conversion of a Source object, borrowed
// or consumed, to Target.
template <typename Target, typename Source>
struct declares_conversion
    : std::bool_constant<
          detail::converts_from<Target, Source, Source &>::value ||
          detail::converts_from<Target, Source, Source &&>::value> {};

// Whether a `From` reference to a Source object, an lvalue or an rvalue,
// converts to Target: by the declared conversion, whose access the reference
// allows, or by the converting constructor.
template <typename Target, typename Source, typename From>
constexpr bool converts() {
  if constexpr (declares_conversion<Target, Source>::value) {
    using required = typename ::dingo::type_conversion_traits<
        Target, Source>::template required_access<From>;
    return std::is_same_v<required, ::dingo::borrow> ||
           std::is_rvalue_reference_v<From>;
  } else {
    return std::is_constructible_v<Target, From>;
  }
}

// Converts `source` to Target; a declared conversion may return an object
// Target is constructed from.
template <typename Target, typename Source> Target convert_to(Source &&source) {
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  if constexpr (declares_conversion<Target, source_type>::value) {
    return Target(::dingo::type_conversion_traits<Target, source_type>::convert(
        std::forward<Source>(source)));
  } else {
    return Target(std::forward<Source>(source));
  }
}

} // namespace dingo::core
