//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <cstddef>
#include <type_traits>

namespace dingo::detail {
// The layout of a type, as memory that places an object of it uses it: its
// size, its alignment, and its destructor, none where it has none.
struct object_layout {
  std::size_t size;
  std::size_t alignment;
  void (*destroy)(void *) noexcept;
};

template <typename T> void destroy_object(void *object) noexcept {
  static_cast<T *>(object)->~T();
}

template <typename T>
inline constexpr object_layout object_layout_v{
    sizeof(T), alignof(T),
    std::is_trivially_destructible_v<T> ? nullptr : &destroy_object<T>};
} // namespace dingo::detail
