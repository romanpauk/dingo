//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/complete_type.h>
#include <dingo/base/type_list.h>
#include <dingo/type/annotated.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
namespace detail {
template <typename Source, typename Target>
inline constexpr bool is_handle_rebindable_v =
    std::is_constructible_v<Target, Source> ||
    !is_default_type_conversion<type_conversion_traits<Target, Source>>::value;
} // namespace detail

// Whether a handle of the storage can be rebound to a handle of the interface.
// Only a pointer-like handle of an object is, so the rebound handle is named
// only for one: a value wrapper of an abstract interface cannot exist, nor can
// an array of it.
template <typename Storage, typename Interface>
inline constexpr bool is_interface_storage_rebindable_v = [] {
  // A handle of an array does not borrow its element.
  if constexpr (type_traits<Storage>::enabled &&
                type_traits<Storage>::is_pointer_like &&
                is_value_borrowable_v<Storage>) {
    return detail::is_handle_rebindable_v<
        Storage,
        rebind_leaf_t<Storage, typename annotated_traits<Interface>::type>>;
  } else {
    return false;
  }
}();

namespace detail {
template <typename Storage, typename InterfaceList>
struct use_interface_as_stored_leaf;

template <typename Storage, typename... Interfaces>
struct use_interface_as_stored_leaf<Storage, type_list<Interfaces...>>
    : std::bool_constant<false> {};

template <typename Storage, typename Interface>
inline constexpr bool can_store_interface_as_leaf_v = [] {
  using interface_type = typename annotated_traits<Interface>::type;
  if constexpr (!requires_complete_type_v<interface_type> ||
                !is_complete_v<interface_type> ||
                !std::is_class_v<interface_type>) {
    return false;
  } else {
    return std::has_virtual_destructor_v<interface_type> &&
           is_interface_storage_rebindable_v<Storage, Interface>;
  }
}();

template <typename Storage, typename Interface>
struct use_interface_as_stored_leaf<Storage, type_list<Interface>>
    : std::bool_constant<can_store_interface_as_leaf_v<Storage, Interface>> {};

template <typename Storage, typename InterfaceList>
inline constexpr bool use_interface_as_stored_leaf_v =
    use_interface_as_stored_leaf<Storage, InterfaceList>::value;
} // namespace detail
} // namespace dingo
