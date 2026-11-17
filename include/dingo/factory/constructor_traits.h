//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/request/operation.h>
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
  // A volatile value is returned without the qualifier, which C++20 does
  // not take on a return type.
  template <typename... Args>
  static std::remove_volatile_t<T> construct(Args &&...args) {
    return T{std::forward<Args>(args)...};
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
    new (ptr) T{std::forward<Args>(args)...};
  }
};

// A pointer is to a new T; one placed at an address is constructed there as T
// is, a wrapper emplaced of the arguments of its element.
template <typename T> struct constructor_traits<T *> {
  template <typename... Args> static T *construct(Args &&...args) {
    return new T{std::forward<Args>(args)...};
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
    if constexpr (type_traits<T>::enabled && !std::is_pointer_v<T>) {
      new (ptr) T(type_traits<T>::emplace(std::forward<Args>(args)...));
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
    return type_traits<T>::emplace(std::forward<Args>(args)...);
  }

  template <typename... Args> static T &construct(T &ptr, Args &&...args) {
    ptr = type_traits<T>::emplace(std::forward<Args>(args)...);
    return ptr;
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
    new (ptr) T(type_traits<T>::emplace(std::forward<Args>(args)...));
  }
};

namespace detail {
template <typename Type>
using construction_result_t =
    std::conditional_t<std::is_pointer_v<Type>, std::remove_pointer_t<Type>,
                       Type>;

// Type made of the Selected type a factory constructs.
template <typename Type, typename Selected> struct converted_constructor {
  using type = construction_result_t<Type>;

  template <typename... Args> static auto construct(Args &&...args) {
    if constexpr (std::is_pointer_v<Type>) {
      return new type(
          execution::make_value<type>(constructor_traits<Selected>::construct(
              std::forward<Args>(args)...)));
    } else {
      return execution::make_value<type>(
          constructor_traits<Selected>::construct(std::forward<Args>(args)...));
    }
  }

  template <typename... Args> static auto construct(void *ptr, Args &&...args) {
    new (ptr) type(execution::make_value<type>(
        constructor_traits<Selected>::construct(std::forward<Args>(args)...)));
  }
};

// Whether type_traits<Wrapper>::emplace passes its arguments to the emplace
// of an element that is a wrapper too, as the wrapper declares with
// emplaces_element.
template <typename Wrapper, typename = void>
inline constexpr bool emplaces_element_v = false;

template <typename Wrapper>
inline constexpr bool emplaces_element_v<
    Wrapper, std::void_t<decltype(type_traits<Wrapper>::emplaces_element)>> =
    type_traits<Wrapper>::emplaces_element;

// Whether Type is emplaced of the arguments of Selected: it is Selected, or a
// wrapper whose element is, down layers that each emplace their element. An
// alternative type selects its alternative of the arguments instead.
template <typename Type, typename Selected> constexpr bool emplaces_selected() {
  if constexpr (std::is_same_v<Type, Selected>) {
    return true;
  } else if constexpr (!type_traits<Type>::enabled || std::is_pointer_v<Type> ||
                       is_alternative_type_v<Type>) {
    return false;
  } else {
    using element = std::remove_cv_t<typename type_traits<Type>::value_type>;
    if constexpr (std::is_same_v<element, Selected>) {
      return true;
    } else if constexpr (emplaces_element_v<Type>) {
      return emplaces_selected<element, Selected>();
    } else {
      return false;
    }
  }
}

// Whether Type is made of the Selected type a factory constructs, when it is
// not emplaced of its arguments in place: an array is made of its elements.
template <typename Type, typename Selected> constexpr bool builds_selected() {
  using type = std::remove_cv_t<construction_result_t<Type>>;
  if constexpr (emplaces_selected<type, Selected>() || std::is_array_v<type>) {
    return false;
  } else {
    return execution::makes_target<type, Selected>();
  }
}

// What constructs Type of the Selected type a factory constructs: Type made
// of Selected where it is, else Type itself.
template <typename Type, typename Selected> struct construction_select {
  using type = std::conditional_t<builds_selected<Type, Selected>(),
                                  converted_constructor<Type, Selected>,
                                  constructor_traits<Type>>;
};

template <typename Type, typename Selected>
using construction_t = typename construction_select<Type, Selected>::type;
} // namespace detail

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
