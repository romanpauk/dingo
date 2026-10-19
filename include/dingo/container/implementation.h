//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <type_traits>

namespace dingo {

namespace detail {

// Containers that keep their state behind a movable handle are used by their
// implementation wherever another container or a binding refers to them.
template <typename T, typename = void> struct container_implementation {
  using type = T;

  static T *get(T *container) noexcept { return container; }
};

template <typename T>
struct container_implementation<T,
                                std::void_t<typename T::implementation_type>> {
  using type = typename T::implementation_type;

  static type *get(T *container) noexcept {
    return container != nullptr ? container->impl_ : nullptr;
  }
};

template <typename T>
using container_implementation_t = typename container_implementation<T>::type;

template <typename T>
container_implementation_t<T> *container_implementation_of(T *container) {
  return container_implementation<T>::get(container);
}

} // namespace detail
} // namespace dingo
