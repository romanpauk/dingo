//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/rtti/typeid_provider.h>

#include <memory>
#include <tuple>
#include <type_traits>

namespace dingo {

// Registering a type whose singular lookup is already taken throws.
struct reject_duplicates {};

// Registering a type whose singular lookup is already taken replaces the lookup
// until the container's first successful resolve seals it. Replaced bindings
// stay owned by the container, only their lookup is redirected.
struct replace_until_resolved {};

struct dynamic_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
  using duplicate_registration_policy = reject_duplicates;
};

namespace detail {

template <typename T, typename = void>
struct container_duplicate_registration_policy {
  using type = reject_duplicates;
};

template <typename T>
struct container_duplicate_registration_policy<
    T, std::void_t<typename T::duplicate_registration_policy>> {
  using type = typename T::duplicate_registration_policy;
};

template <typename T>
using container_duplicate_registration_policy_t =
    typename container_duplicate_registration_policy<T>::type;

template <typename T, typename = void>
struct is_runtime_container_traits : std::false_type {};

template <typename T>
struct is_runtime_container_traits<
    T, std::void_t<typename T::rtti_type, typename T::allocator_type,
                   container_lookup_definition_type_t<T>>> : std::true_type {};

template <typename T>
inline constexpr bool is_runtime_container_traits_v =
    is_runtime_container_traits<T>::value;

} // namespace detail
} // namespace dingo
