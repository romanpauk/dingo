//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/meta.h>
#include <dingo/base/type_list.h>
#include <dingo/type/annotated.h>
#include <dingo/type/shape.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
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
    using rebound = rebind_leaf_t<Storage, unannotated_t<Interface>>;
    return std::is_constructible_v<rebound, Storage> ||
           !detail::is_default_type_conversion<
               type_conversion_traits<rebound, Storage>>::value;
  } else {
    return false;
  }
}();

namespace detail {
// Whether a registration of one interface stores its handle as one of the
// interface: a class with a virtual destructor the handle rebinds to.
template <typename Storage, typename InterfaceList>
inline constexpr bool use_interface_as_stored_leaf_v = false;

template <typename Storage, typename Interface>
inline constexpr bool
    use_interface_as_stored_leaf_v<Storage, type_list<Interface>> = [] {
      using interface_type = unannotated_t<Interface>;
      if constexpr (!requires_complete_type_v<interface_type> ||
                    !is_complete_v<interface_type> ||
                    !std::is_class_v<interface_type>) {
        return false;
      } else {
        return std::has_virtual_destructor_v<interface_type> &&
               is_interface_storage_rebindable_v<Storage, Interface>;
      }
    }();
} // namespace detail
} // namespace dingo
