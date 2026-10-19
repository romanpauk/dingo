//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace dingo {
template <typename T, typename = void> struct alternative_type_traits {
  static constexpr bool enabled = false;
};

template <typename T>
inline constexpr bool is_alternative_type_v =
    alternative_type_traits<std::remove_cv_t<T>>::enabled;

namespace detail {
template <typename List, typename Selected> struct type_list_count;

template <typename Selected, typename... Alternatives>
struct type_list_count<type_list<Alternatives...>, Selected>
    : std::integral_constant<
          size_t,
          (0u + ... + (std::is_same_v<Selected, Alternatives> ? 1u : 0u))> {};

template <typename List> struct type_list_has_duplicates;

template <typename... Alternatives>
struct type_list_has_duplicates<type_list<Alternatives...>>
    : std::bool_constant<((type_list_count<type_list<Alternatives...>,
                                           Alternatives>::value > 1) ||
                          ...)> {};

template <typename Type, typename = void>
struct alternative_type_alternatives {};

template <typename Type>
struct alternative_type_alternatives<
    Type, std::enable_if_t<
              alternative_type_traits<std::remove_cv_t<Type>>::enabled>> {
  using type =
      typename alternative_type_traits<std::remove_cv_t<Type>>::alternatives;

  static_assert(!type_list_has_duplicates<type>::value,
                "alternative_type_traits<T>::alternatives must not contain "
                "duplicate types");
};

template <typename Type>
using alternative_type_alternatives_t =
    typename alternative_type_alternatives<Type>::type;

template <typename T> struct variant_conversion_array {
  T value[1];
};

template <typename Alternative, typename Source, typename = void>
struct variant_conversion_candidate {
  static void select();
};

template <typename Alternative, typename Source>
struct variant_conversion_candidate<
    Alternative, Source,
    std::void_t<decltype(variant_conversion_array<Alternative>{
        {std::declval<Source>()}})>> {
  static type_list_iterator<Alternative> select(Alternative);
};

template <typename Source, typename... Alternatives>
struct variant_conversion_candidates
    : variant_conversion_candidate<Alternatives, Source>... {
  using variant_conversion_candidate<Alternatives, Source>::select...;
};

template <typename Source, typename... Alternatives>
using selected_variant_alternative_t =
    typename decltype(variant_conversion_candidates<Source, Alternatives...>::
                          select(std::declval<Source>()))::type;

template <typename Type, typename Selected, typename = void>
struct alternative_type_count : std::integral_constant<size_t, 0> {};

template <typename Type, typename Selected>
struct alternative_type_count<
    Type, Selected,
    std::enable_if_t<alternative_type_traits<std::remove_cv_t<Type>>::enabled>>
    : type_list_count<alternative_type_alternatives_t<Type>,
                      std::remove_cv_t<Selected>> {};

template <typename Type, typename = void>
struct alternative_type_interface_types {};

template <typename Type>
struct alternative_type_interface_types<
    Type, std::enable_if_t<
              alternative_type_traits<std::remove_cv_t<Type>>::enabled>> {
  using type = type_list_cat_t<type_list<std::remove_cv_t<Type>>,
                               alternative_type_alternatives_t<Type>>;
};
} // namespace detail

template <typename... Alternatives>
struct alternative_type_traits<std::variant<Alternatives...>> {
  static constexpr bool enabled = true;

  using alternatives = type_list<Alternatives...>;

  template <typename Source>
  using selected_type =
      detail::selected_variant_alternative_t<Source, Alternatives...>;

  template <typename Selected, typename Value>
  static std::variant<Alternatives...> wrap(Value &&value) {
    return std::variant<Alternatives...>(std::in_place_type<Selected>,
                                         std::forward<Value>(value));
  }

  template <typename Selected>
  static Selected *get(std::variant<Alternatives...> &value) {
    return std::get_if<Selected>(&value);
  }

  template <typename Selected>
  static const Selected *get(const std::variant<Alternatives...> &value) {
    return std::get_if<Selected>(&value);
  }
};

template <typename Type, typename Interface>
inline constexpr bool is_alternative_type_interface_compatible_v =
    detail::alternative_type_count<std::remove_cv_t<Type>,
                                   std::remove_cv_t<Interface>>::value == 1;
} // namespace dingo
