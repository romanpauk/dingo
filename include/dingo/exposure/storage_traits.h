//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>

namespace dingo {
template <typename StorageTag, typename Type, typename U, typename = void>
struct storage_traits {
  static constexpr bool enabled = false;
  static constexpr bool is_stable = false;

  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<>;
};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, const Type, U>
    : storage_traits<StorageTag, Type, U> {};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, Type &, U>
    : storage_traits<StorageTag, Type, U> {};

template <typename StorageTag, typename Type, typename U>
struct storage_traits<StorageTag, const Type &, U>
    : storage_traits<StorageTag, Type, U> {};
} // namespace dingo
