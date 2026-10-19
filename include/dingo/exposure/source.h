//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/conversion/type_conversion_traits.h>

#include <type_traits>

namespace dingo {
namespace detail {
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Storage, typename = void> struct storage_source_type {
  using type = std::remove_reference_t<typename Storage::type>;
};

template <typename Storage>
struct storage_source_type<Storage,
                           std::void_t<typename Storage::resolved_type>> {
  using type = std::remove_reference_t<typename Storage::resolved_type>;
};

template <typename Storage>
using storage_source_type_t = typename storage_source_type<Storage>::type;

// The source helpers below read a storage_shape (defined further down).
template <typename Storage> struct storage_borrow_source {
private:
  using source_type = typename Storage::resolved_type;

public:
  using type = std::conditional_t<std::is_pointer_v<source_type>, source_type,
                                  source_type &>;
};

template <typename Storage>
using storage_borrow_source_t = typename storage_borrow_source<Storage>::type;

template <typename Storage>
using storage_consume_source_t = typename Storage::resolved_type &&;

template <typename Storage> struct storage_source_traits {
private:
  using stored_value_type = remove_cvref_t<typename Storage::type>;
  using resolved_type = typename Storage::resolved_type;
  using resolved_leaf_type =
      std::conditional_t<std::is_pointer_v<resolved_type>,
                         std::remove_pointer_t<resolved_type>, resolved_type>;

public:
  using stored_type = stored_value_type;
  using direct_leaf_type =
      std::remove_cv_t<std::remove_pointer_t<stored_value_type>>;
  using borrowed_type = storage_borrow_source_t<Storage>;
  using consumed_type = storage_consume_source_t<Storage>;
  using qualification_type = resolved_leaf_type;

  static constexpr bool is_pointer = std::is_pointer_v<stored_value_type>;
  static constexpr bool is_const = std::is_const_v<resolved_leaf_type>;

  template <typename Interface>
  using interface_type = copy_cv_t<resolved_leaf_type, Interface>;

  template <typename Interface>
  static constexpr bool publishes_interface =
      !is_pointer ||
      std::is_convertible_v<stored_value_type, interface_type<Interface> *>;
};
} // namespace detail
} // namespace dingo
