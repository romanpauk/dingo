//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <new>
#include <utility>

namespace dingo {
namespace detail {
template <typename T, size_t N, typename... Args>
T *make_bounded_array(Args &&...args) {
  static_assert(sizeof...(Args) <= N,
                "too many initializers for bounded array construction");
  return new T[N]{std::forward<Args>(args)...};
}

template <typename T, size_t N, typename... Args>
void construct_bounded_array(void *ptr, Args &&...args) {
  static_assert(sizeof...(Args) <= N,
                "too many initializers for bounded array construction");
  new (ptr) T[N]{std::forward<Args>(args)...};
}

template <typename T, typename... Args> T *make_dynamic_array(Args &&...args) {
  static_assert(sizeof...(Args) > 0,
                "dynamic arrays require an explicit size via a custom "
                "factory or element initializers");
  return new T[sizeof...(Args)]{std::forward<Args>(args)...};
}
} // namespace detail
} // namespace dingo
