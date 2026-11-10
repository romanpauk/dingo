//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/type/type_traits.h>

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

// A specialization may also declare how detection sees the type:
//
//   arguments            the type_list of the arguments it is constructed of
//                        when it has one fixed signature, which detection
//                        takes instead of searching for a constructor
//   initializable<Args>  whether it is list-initialized of Args, where probing
//                        T{args...} would not tell, as for a nested array
//
// The overloads that construct in place are declared with a deduced `auto`
// return type, which is `void`. A deduced return type is instantiated at the
// call, so the construction of a dependency does not add template depth levels
// to the dependency chain it is part of (see docs/advanced-topics.md).
template <typename T, typename = void> struct constructor_traits {
  template <typename... Args> static T construct(Args &&...args) {
    return T{std::forward<Args>(args)...};
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
    new (ptr) T{std::forward<Args>(args)...};
  }
};

// A pointer is to a new T; one placed at an address is constructed there as T
// is, a wrapper made of what makes its element.
template <typename T> struct constructor_traits<T *> {
  template <typename... Args> static T *construct(Args &&...args) {
    return new T{std::forward<Args>(args)...};
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>) {
      new (ptr) T(type_traits<T>::make(std::forward<Args>(args)...));
    } else {
      new (ptr) T{std::forward<Args>(args)...};
    }
  }
};

template <typename T, size_t N> struct constructor_traits<T[N]> {
  template <typename... Args> static T *construct(Args &&...args) {
    return detail::make_bounded_array<T, N>(std::forward<Args>(args)...);
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
    detail::construct_bounded_array<T, N>(ptr, std::forward<Args>(args)...);
  }
};

namespace detail {
template <typename T, typename Sequence> struct repeated_arguments;

template <typename T, std::size_t... Is>
struct repeated_arguments<T, std::index_sequence<Is...>> {
  using type = type_list<
      std::conditional_t<true, T, std::integral_constant<std::size_t, Is>>...>;
};
} // namespace detail

// std::array stores its elements in a nested native array: it is made of its
// elements with the braces the nested array requires, which MSVC does not
// elide while probing, and it has the one signature of its N elements.
template <typename T, std::size_t N>
struct constructor_traits<std::array<T, N>> {
  using arguments =
      typename detail::repeated_arguments<T, std::make_index_sequence<N>>::type;
  template <typename... Args>
  static constexpr bool initializable =
      detail::is_bounded_array_initializable_v<T, N, Args...>;

  template <typename... Args>
  static std::array<T, N> construct(Args &&...args) {
    return {{std::forward<Args>(args)...}};
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
#if defined(_MSC_VER)
    // MSVC rejects direct placement-new aggregate initialization for nested
    // arrays of move-only elements. Constructing from the returned prvalue uses
    // C++17 guaranteed copy elision and does not require a copy or move.
    new (ptr) std::array<T, N>(construct(std::forward<Args>(args)...));
#else
    new (ptr) std::array<T, N>{{std::forward<Args>(args)...}};
#endif
  }
};

template <typename T> struct constructor_traits<T &> {
  // References cannot be constructed.
  template <typename... Args> static T &construct(Args &&...) = delete;
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

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
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

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
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

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
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
