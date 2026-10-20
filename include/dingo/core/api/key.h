//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The keys of the public API of the next implementation: key_type, key_value,
// and dependency, a constructor parameter that requests its type by a key.

#include <dingo/core/projection/key.h> // IWYU pragma: export

#include <type_traits>

namespace dingo {

template <typename Type, typename Key> struct dependency_type {
  using type = core::keyed<Type, core::key_of_t<Key>>;
};

template <typename Type> struct dependency_type<Type, key_type<none_t>> {
  using type = Type;
};

template <typename Type, typename Key = key_type<none_t>>
using dependency = typename dependency_type<Type, std::decay_t<Key>>::type;

} // namespace dingo
