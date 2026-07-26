//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
template <typename TargetType, typename SourceType, typename = void>
struct type_conversion_traits {
  template <typename Source>
  static constexpr bool enabled = std::is_constructible_v<TargetType, Source>;

  template <typename Source, typename Result = TargetType>
  static std::enable_if_t<!std::is_array_v<Result>, Result>
  convert(Source &&source) {
    static_assert(enabled<Source &&>,
                  "type conversion requires a type_conversion_traits "
                  "specialization or a direct converting constructor");
    return Result(std::forward<Source>(source));
  }
};

template <typename Target, typename Source>
struct type_conversion_traits<Target *, Source *> {
  template <typename ActualSource>
  static constexpr bool enabled = std::is_convertible_v<ActualSource, Target *>;

  static Target *convert(Source *source) {
    static_assert(enabled<Source *>,
                  "pointer conversion requires an implicit conversion");
    return static_cast<Target *>(source);
  }
};

namespace detail {
template <typename Target, typename Source> struct is_type_conversion_available;

template <typename Traits, typename Source, typename = void>
struct has_declared_conversion_availability : std::false_type {};

template <typename Traits, typename Source>
struct has_declared_conversion_availability<
    Traits, Source, std::void_t<decltype(Traits::template enabled<Source>)>>
    : std::true_type {};

template <typename Target, typename Traits, typename Source, typename = void>
struct has_compatible_conversion : std::false_type {};

template <typename Target, typename Traits, typename Source>
struct has_compatible_conversion<
    Target, Traits, Source,
    std::void_t<decltype(Traits::convert(std::declval<Source>()))>>
    : std::bool_constant<std::is_constructible_v<
          Target, decltype(Traits::convert(std::declval<Source>()))>> {};

template <typename Target, typename Traits, typename Source,
          bool HasDeclaredAvailability =
              has_declared_conversion_availability<Traits, Source>::value>
struct is_declared_type_conversion_available
    : has_compatible_conversion<Target, Traits, Source> {};

template <typename Target, typename Traits, typename Source>
struct is_declared_type_conversion_available<Target, Traits, Source, true>
    : std::bool_constant<Traits::template enabled<Source>> {};

template <typename Target, typename Source, typename = void>
struct is_borrowed_type_conversion_available : std::false_type {};

template <typename Target, typename Source>
struct is_borrowed_type_conversion_available<
    Target, Source,
    std::enable_if_t<
        type_traits<std::remove_cv_t<std::remove_reference_t<Source>>>::
            is_value_borrowable,
        std::void_t<
            decltype(type_traits<std::remove_cv_t<std::remove_reference_t<
                         Source>>>::borrow(std::declval<Source>()))>>>
    : is_type_conversion_available<
          Target, decltype(type_traits<std::remove_cv_t<std::remove_reference_t<
                               Source>>>::borrow(std::declval<Source>()))> {};

template <typename Target, typename Source>
struct is_alternative_type_conversion_available {
private:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_reference_t<Source>;
  using qualified_target_type =
      std::conditional_t<std::is_const_v<source_type>, const target_type,
                         target_type>;
  using consumed_alternative_type = qualified_target_type &&;

public:
  static constexpr bool value =
      alternative_type_count<std::remove_cv_t<source_type>,
                             target_type>::value == 1 &&
      (std::is_lvalue_reference_v<Source>
           ? is_copy_constructible_v<target_type>
           : std::is_constructible_v<target_type, consumed_alternative_type>);
};

template <typename Target, typename Source>
struct is_type_conversion_available
    : std::bool_constant<
          is_declared_type_conversion_available<
              Target,
              type_conversion_traits<
                  Target, std::remove_cv_t<std::remove_reference_t<Source>>>,
              Source>::value ||
          is_borrowed_type_conversion_available<Target, Source>::value ||
          is_alternative_type_conversion_available<Target, Source>::value> {};

template <typename Target, typename Source>
inline constexpr bool is_type_conversion_available_v =
    is_type_conversion_available<Target, Source>::value;
} // namespace detail
} // namespace dingo
