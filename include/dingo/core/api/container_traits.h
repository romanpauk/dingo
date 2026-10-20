//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The traits of the containers of the next implementation: the lookup
// declarations they list, none by default.

#include <dingo/core/api/rtti/typeid_provider.h>

#include <memory>
#include <tuple>

namespace dingo {

struct dynamic_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using allocator_type = std::allocator<char>;
  using lookup_definition_type = std::tuple<>;
};

struct static_container_traits {
  using rtti_type = rtti<typeid_provider>;
  using lookup_definition_type = std::tuple<>;
};

namespace detail {
// The member types a container takes from its traits: their own, or the
// ones of dynamic_container_traits.
template <typename Traits, typename = void> struct traits_rtti {
  using type = dynamic_container_traits::rtti_type;
};

template <typename Traits>
struct traits_rtti<Traits, std::void_t<typename Traits::rtti_type>> {
  using type = typename Traits::rtti_type;
};

template <typename Traits, typename = void> struct traits_allocator {
  using type = dynamic_container_traits::allocator_type;
};

template <typename Traits>
struct traits_allocator<Traits, std::void_t<typename Traits::allocator_type>> {
  using type = typename Traits::allocator_type;
};

template <typename Type, typename = void>
struct is_container_type : std::false_type {};

template <typename Type>
struct is_container_type<Type, std::void_t<decltype(Type::has_static_source)>>
    : std::true_type {};

// Whether a type is container traits: it declares lookups and is not a
// container, which exposes the lookups of its traits.
template <typename Type, typename = void>
struct is_traits_type : std::false_type {};

template <typename Type>
struct is_traits_type<Type, std::void_t<typename Type::lookup_definition_type>>
    : std::bool_constant<!is_container_type<Type>::value> {};

// The member types of a container of `Traits`.
template <typename Traits> struct container_member_types {
  using traits_type = Traits;
  using lookup_definition_type = typename Traits::lookup_definition_type;
  using rtti_type = typename traits_rtti<Traits>::type;
  using allocator_type = typename traits_allocator<Traits>::type;
};
} // namespace detail

} // namespace dingo
