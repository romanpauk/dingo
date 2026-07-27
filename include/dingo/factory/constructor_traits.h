//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

template <typename T, typename = void> struct constructor_traits {
  template <typename... Args> static T construct(Args &&...args) {
    return T{std::forward<Args>(args)...};
  }

  template <typename... Args> static void construct(void *ptr, Args &&...args) {
    new (ptr) T{std::forward<Args>(args)...};
  }
};

template <typename T> struct constructor_traits<T *> {
  template <typename... Args> static T *construct(Args &&...args) {
    return new T{std::forward<Args>(args)...};
  }

  template <typename... Args> static void construct(void *ptr, Args &&...args) {
    new (ptr) T{std::forward<Args>(args)...};
  }
};

template <typename T, size_t N> struct constructor_traits<T[N]> {
  template <typename... Args> static T *construct(Args &&...args) {
    return detail::make_bounded_array<T, N>(std::forward<Args>(args)...);
  }

  template <typename... Args> static void construct(void *ptr, Args &&...args) {
    detail::construct_bounded_array<T, N>(ptr, std::forward<Args>(args)...);
  }
};

template <typename T> struct constructor_traits<T &> {
  template <typename... Args> static T &construct(Args &&...) {
    static_assert(true, "references cannot be constructed");
  }
};

template <typename T>
struct constructor_traits<
    T, std::enable_if_t<type_traits<T>::enabled && !std::is_pointer_v<T>>> {
  template <typename... Args> static T construct(Args &&...args) {
    return type_traits<T>::make(std::forward<Args>(args)...);
  }

  template <typename... Args> static T &construct(T &ptr, Args &&...args) {
    ptr = type_traits<T>::make(std::forward<Args>(args)...);
    return ptr;
  }

  template <typename... Args> static void construct(void *ptr, Args &&...args) {
    new (ptr) T(type_traits<T>::make(std::forward<Args>(args)...));
  }
};

namespace detail {
template <typename Type>
using construction_result_t =
    std::conditional_t<std::is_pointer_v<Type>, std::remove_pointer_t<Type>,
                       Type>;

template <typename Type, typename Selected>
using construction_conversion_t =
    type_conversion_path_t<construction_result_t<Type>, Selected &&, consume>;

template <typename Type, typename Selected,
          typename Conversion = construction_conversion_t<Type, Selected>>
struct construction_dispatch {
  template <typename... Args> static auto construct(Args &&...args) {
    return constructor_traits<Type>::construct(std::forward<Args>(args)...);
  }

  template <typename... Args> static void construct(void *ptr, Args &&...args) {
    constructor_traits<Type>::construct(ptr, std::forward<Args>(args)...);
  }
};

template <typename Type, typename Selected, typename Conversion>
struct construction_dispatch<Type, Selected,
                             converted_construction<Conversion>> {
  using type = construction_result_t<Type>;

  template <typename... Args> static auto construct(Args &&...args) {
    if constexpr (std::is_pointer_v<Type>) {
      return new type(convert_type<type>(
          constructor_traits<Selected>::construct(std::forward<Args>(args)...),
          Conversion{}));
    } else {
      return convert_type<type>(
          constructor_traits<Selected>::construct(std::forward<Args>(args)...),
          Conversion{});
    }
  }

  template <typename... Args> static void construct(void *ptr, Args &&...args) {
    new (ptr) type(convert_type<type>(
        constructor_traits<Selected>::construct(std::forward<Args>(args)...),
        Conversion{}));
  }
};
} // namespace detail

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
