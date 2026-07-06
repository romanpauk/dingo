//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/key.h>

#include <type_traits>

namespace dingo {

namespace detail {

template <typename T, typename = void>
struct has_lookup_definition_type : std::false_type {};

template <typename T>
struct has_lookup_definition_type<
    T, std::void_t<typename T::lookup_definition_type>> : std::true_type {};

template <typename T,
          bool HasLookupDefinition = has_lookup_definition_type<T>::value>
struct container_lookup_definition_type {};

template <typename T> struct container_lookup_definition_type<T, true> {
  using type = typename T::lookup_definition_type;
};

template <typename T>
using container_lookup_definition_type_t =
    typename container_lookup_definition_type<T>::type;

} // namespace detail
} // namespace dingo
