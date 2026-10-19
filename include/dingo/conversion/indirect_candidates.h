//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/conversion/conversions.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename Target, typename Source, typename Access, typename = void>
struct borrowed_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access, typename = void>
struct dereference_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source>
struct array_type_conversion_candidate {
private:
  using target_type = std::remove_reference_t<Target>;
  using source_type = std::remove_reference_t<Source>;
  using target_array =
      std::conditional_t<std::is_pointer_v<Target>,
                         std::remove_pointer_t<Target>, target_type>;
  using projected_element = std::remove_extent_t<target_array>;
  using source_element = std::remove_pointer_t<source_type>;
  static constexpr bool same_projection =
      std::is_same_v<std::remove_cv_t<source_element>,
                     std::remove_cv_t<projected_element>>;
  static constexpr bool compatible_projection =
      std::is_pointer_v<source_type> && same_projection &&
      std::is_convertible_v<std::add_pointer_t<source_element>,
                            std::add_pointer_t<projected_element>>;
  static constexpr bool reference = std::is_lvalue_reference_v<Target> &&
                                    std::is_array_v<target_type> &&
                                    compatible_projection;
  static constexpr bool pointer =
      std::is_pointer_v<Target> &&
      std::is_array_v<std::remove_pointer_t<Target>> && compatible_projection;

public:
  using type = std::conditional_t<
      reference, array_reference_type_conversion<Target, Source>,
      std::conditional_t<pointer, array_pointer_type_conversion<Target, Source>,
                         unavailable_type_conversion>>;
};

template <typename Target, typename Source, typename = void>
struct pointer_access_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access, typename = void>
struct retained_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target>
using retained_conversion_type_t = std::remove_cv_t<
    std::conditional_t<std::is_pointer_v<Target>, std::remove_pointer_t<Target>,
                       std::remove_reference_t<Target>>>;

template <typename Target, typename Source>
struct pointer_access_type_conversion_candidate<
    Target, Source,
    std::void_t<decltype(type_traits<std::remove_cv_t<std::remove_reference_t<
                             Source>>>::get(std::declval<Source>()))>> {
private:
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using result_type =
      decltype(type_traits<source_type>::get(std::declval<Source>()));

public:
  using type =
      std::conditional_t<std::is_pointer_v<Target> &&
                             std::is_lvalue_reference_v<Source> &&
                             std::is_convertible_v<result_type, Target>,
                         pointer_access_type_conversion<Target, Source>,
                         unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access>
struct borrowed_type_conversion_candidate<
    Target, Source, Access,
    std::enable_if_t<
        type_traits<std::remove_cv_t<std::remove_reference_t<Source>>>::
            is_value_borrowable,
        std::void_t<
            decltype(type_traits<std::remove_cv_t<std::remove_reference_t<
                         Source>>>::borrow(std::declval<Source>()))>>> {
private:
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using borrowed_source =
      decltype(type_traits<source_type>::borrow(std::declval<Source>()));
  using next =
      typename type_conversion_path<Target, borrowed_source, Access>::type;

public:
  using type =
      std::conditional_t<next::available,
                         borrowed_type_conversion<Target, Source, next>,
                         unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access>
struct dereference_type_conversion_candidate<
    Target, Source, Access,
    std::enable_if_t<std::is_pointer_v<std::remove_reference_t<Source>> &&
                     !std::is_void_v<std::remove_pointer_t<
                         std::remove_reference_t<Source>>> &&
                     !std::is_pointer_v<Target>>> {
private:
  using pointee_source = decltype(*std::declval<Source>());
  using conversion =
      typename type_conversion_path<Target, pointee_source, Access>::type;

public:
  using type = std::conditional_t<
      conversion::available,
      dereference_type_conversion<Target, Source, conversion>,
      unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access>
struct retained_type_conversion_candidate<
    Target, Source, Access,
    std::enable_if_t<(std::is_lvalue_reference_v<Target> ||
                      std::is_pointer_v<Target>) &&
                     type_traits<retained_conversion_type_t<Target>>::enabled &&
                     !std::is_array_v<retained_conversion_type_t<Target>>>> {
private:
  using retained_type = retained_conversion_type_t<Target>;
  using conversion =
      typename type_conversion_path<retained_type, Source, Access>::type;

public:
  using type =
      std::conditional_t<conversion::available,
                         retained_type_conversion<Target, Source, conversion>,
                         unavailable_type_conversion>;
};
} // namespace detail
} // namespace dingo
