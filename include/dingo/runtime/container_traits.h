//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/tags.h>
#include <dingo/rtti/typeid_provider.h>

#include <memory>
#include <type_traits>

namespace dingo {

// Registering a type whose singular lookup is already taken throws.
struct reject_duplicates {};

// Registering a type whose singular lookup is already taken replaces the lookup
// until the container's first successful resolve seals it. Replaced bindings
// stay owned by the container, only their lookup is redirected.
struct replace_until_resolved {};

struct runtime_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = lookups<>;
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

} // namespace detail
} // namespace dingo
