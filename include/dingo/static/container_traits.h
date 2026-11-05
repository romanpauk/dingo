//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/tags.h>
#include <dingo/registration/type_registration.h>
#include <dingo/rtti/static_provider.h>

#include <memory>
#include <type_traits>

namespace dingo {

struct static_container_traits {
  using rtti_type = rtti<static_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = lookups<>;
};

template <typename StaticSource, typename ParentContainer = void>
class static_container;

namespace detail {

// The traits a static container takes from its parent: those of a parent
// container, or the default static traits.
template <typename T, typename = void> struct container_traits_of {
  using type = ::dingo::static_container_traits;
};

template <typename T>
struct container_traits_of<T, std::void_t<typename T::container_traits_type>> {
  using type = typename T::container_traits_type;
};

template <typename T>
using container_traits_of_t = typename container_traits_of<T>::type;

} // namespace detail
} // namespace dingo
