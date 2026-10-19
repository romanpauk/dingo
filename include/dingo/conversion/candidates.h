//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/conversion/conversions.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename Alternative, typename Source>
using qualified_alternative_t =
    copy_cv_t<std::remove_reference_t<Source>, Alternative>;

template <typename Alternative, typename Source>
using alternative_source_t =
    std::conditional_t<std::is_lvalue_reference_v<Source>,
                       qualified_alternative_t<Alternative, Source> &,
                       qualified_alternative_t<Alternative, Source> &&>;

template <typename Type>
inline constexpr bool is_value_wrapper_type_v = [] {
  using type = std::remove_cv_t<std::remove_reference_t<Type>>;
  return type_traits<type>::enabled && !type_traits<type>::is_pointer_like &&
         has_value_type_v<type>;
}();

template <typename Target>
inline constexpr bool is_structural_conversion_target_v = [] {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  return !std::is_reference_v<Target> && !std::is_pointer_v<Target> &&
         (is_alternative_type_v<target_type> ||
          is_value_wrapper_type_v<target_type>);
}();

// Default conversions into wrappers and alternatives are recovered by the
// structural candidates below. Once the basic identity/reference cases have
// failed, no direct conversion can win for these targets.
struct default_structural_conversion_path {
  static constexpr bool uses_default_conversion = true;
  static constexpr bool can_take_address = false;
  static constexpr bool available = false;
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
using conversion_direct_path_t = std::conditional_t<
    is_structural_conversion_target_v<Target> &&
        is_default_type_conversion<type_conversion_traits<
            Target, std::remove_cv_t<std::remove_reference_t<Source>>>>::value,
    default_structural_conversion_path,
    direct_type_conversion_path<Target, Source, Access>>;

template <typename Type, typename = void>
struct borrows_value_wrapper : std::false_type {};

template <typename Type>
struct borrows_value_wrapper<
    Type,
    std::void_t<decltype(type_traits<std::remove_cv_t<std::remove_reference_t<
                             Type>>>::borrow(std::declval<Type>()))>>
    : std::bool_constant<
          type_traits<std::remove_cv_t<std::remove_reference_t<Type>>>::
              is_value_borrowable &&
          is_value_wrapper_type_v<
              decltype(type_traits<std::remove_cv_t<std::remove_reference_t<
                           Type>>>::borrow(std::declval<Type>()))>> {};

template <typename Type>
inline constexpr bool borrows_value_wrapper_v =
    borrows_value_wrapper<Type>::value;

template <typename Target, typename Source, typename Conversion,
          bool UnwrapSource>
struct compose_wrapper_type_conversion {
  using type =
      wrapper_type_conversion<Target, Source, Conversion, UnwrapSource>;
};

template <typename Target, typename Source, typename Conversion,
          bool UnwrapSource>
struct compose_wrapper_type_conversion<
    Target, Source, converted_construction<Conversion>, UnwrapSource> {
  using type = converted_construction<
      wrapper_type_conversion<Target, Source, Conversion, UnwrapSource>>;
};

template <typename Target, typename Source, typename Conversion,
          bool UnwrapSource>
using compose_wrapper_type_conversion_t =
    typename compose_wrapper_type_conversion<Target, Source, Conversion,
                                             UnwrapSource>::type;

template <typename Target, typename Source, typename Access, typename = void>
struct target_alternative_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
struct target_alternative_type_conversion_candidate<
    Target, Source, Access,
    std::void_t<
        typename alternative_type_traits<std::remove_cv_t<
            std::remove_reference_t<Target>>>::template selected_type<Source>,
        decltype(alternative_type_traits<
                 std::remove_cv_t<std::remove_reference_t<Target>>>::
                     template wrap<typename alternative_type_traits<
                         std::remove_cv_t<std::remove_reference_t<Target>>>::
                                       template selected_type<Source>>(
                         std::declval<
                             typename alternative_type_traits<std::remove_cv_t<
                                 std::remove_reference_t<Target>>>::
                                 template selected_type<Source>>()))>> {
private:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using selected = typename alternative_type_traits<
      target_type>::template selected_type<Source>;
  using conversion =
      typename type_conversion_path<selected, Source, Access>::type;

public:
  using type = std::conditional_t<
      !std::is_reference_v<Target> && !std::is_pointer_v<Target> &&
          is_alternative_type_v<target_type> && conversion::available,
      converted_construction<target_alternative_type_conversion<
          Target, Source, selected, conversion>>,
      unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access,
          bool UnwrapSource = is_value_wrapper_type_v<Source>, typename = void>
struct target_wrapper_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
struct target_wrapper_type_conversion_candidate<
    Target, Source, Access, false,
    std::void_t<
        typename type_traits<
            std::remove_cv_t<std::remove_reference_t<Target>>>::value_type,
        decltype(type_traits<
                 std::remove_cv_t<std::remove_reference_t<Target>>>::
                     wrap(
                         std::declval<typename type_traits<std::remove_cv_t<
                             std::remove_reference_t<Target>>>::value_type>())),
        decltype(type_traits<std::remove_cv_t<
                     std::remove_reference_t<Target>>>::make_empty())>> {
private:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using value_type = typename type_traits<target_type>::value_type;
  using conversion =
      typename type_conversion_path<value_type, Source, Access>::type;

public:
  using type = std::conditional_t<
      !std::is_reference_v<Target> && !std::is_pointer_v<Target> &&
          is_value_wrapper_type_v<target_type> &&
          !borrows_value_wrapper_v<Source> && conversion::available,
      compose_wrapper_type_conversion_t<Target, Source, conversion, false>,
      unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access>
struct target_wrapper_type_conversion_candidate<
    Target, Source, Access, true,
    std::void_t<
        typename type_traits<
            std::remove_cv_t<std::remove_reference_t<Target>>>::value_type,
        typename type_traits<
            std::remove_cv_t<std::remove_reference_t<Source>>>::value_type,
        decltype(type_traits<
                 std::remove_cv_t<std::remove_reference_t<Target>>>::
                     wrap(
                         std::declval<typename type_traits<std::remove_cv_t<
                             std::remove_reference_t<Target>>>::value_type>())),
        decltype(type_traits<std::remove_cv_t<
                     std::remove_reference_t<Target>>>::make_empty()),
        decltype(type_traits<
                 std::remove_cv_t<std::remove_reference_t<Source>>>::
                     empty(std::declval<const std::remove_cv_t<
                               std::remove_reference_t<Source>> &>())),
        decltype(type_traits<
                 std::remove_cv_t<std::remove_reference_t<Source>>>::
                     get(std::declval<std::remove_reference_t<Source> &>()))>> {
private:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using value_type = typename type_traits<target_type>::value_type;
  using source_value_type =
      copy_cv_t<std::remove_reference_t<Source>,
                typename type_traits<source_type>::value_type>;
  using source_value =
      std::conditional_t<std::is_lvalue_reference_v<Source>,
                         source_value_type &, source_value_type &&>;
  using conversion =
      typename type_conversion_path<value_type, source_value, Access>::type;

public:
  using type = std::conditional_t<
      !std::is_reference_v<Target> && !std::is_pointer_v<Target> &&
          is_value_wrapper_type_v<target_type> && conversion::available,
      compose_wrapper_type_conversion_t<Target, Source, conversion, true>,
      unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access,
          typename Alternative>
using alternative_type_conversion_branch_t = alternative_type_conversion_branch<
    Alternative,
    typename type_conversion_path<
        Target, alternative_source_t<Alternative, Source>, Access>::type>;

template <
    typename Target, typename Source, typename Access, typename Alternatives,
    bool Borrows = std::is_reference_v<Target> || std::is_pointer_v<Target>>
struct alternative_type_conversion_candidate;

template <typename Target, typename Source, typename Access,
          typename... Alternatives>
struct alternative_type_conversion_candidate<Target, Source, Access,
                                             type_list<Alternatives...>, true> {
  using type = alternative_type_conversion<
      Target, Source,
      type_list<alternative_type_conversion_branch_t<Target, Source, Access,
                                                     Alternatives>...>>;
};

template <typename Target, typename Source, typename Access, typename Selected,
          bool HasSelected =
              (alternative_type_count<
                   std::remove_cv_t<std::remove_reference_t<Source>>,
                   Selected>::value == 1)>
struct selected_alternative_type_conversion {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access, typename Selected>
struct selected_alternative_type_conversion<Target, Source, Access, Selected,
                                            true> {
  using type = alternative_type_conversion<
      Target, Source,
      type_list<alternative_type_conversion_branch_t<Target, Source, Access,
                                                     Selected>>>;
};

template <typename Target, typename Source, typename Access,
          typename... Alternatives>
struct alternative_type_conversion_candidate<Target, Source, Access,
                                             type_list<Alternatives...>, false>
    : selected_alternative_type_conversion<
          Target, Source, Access,
          std::remove_cv_t<std::remove_reference_t<Target>>> {};

template <typename Target, typename Source, typename Access,
          bool IsAlternative =
              is_alternative_type_v<
                  std::remove_cv_t<std::remove_reference_t<Source>>> &&
              !std::is_volatile_v<std::remove_reference_t<Source>>>
struct source_alternative_type_conversion {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
struct source_alternative_type_conversion<Target, Source, Access, true>
    : alternative_type_conversion_candidate<
          Target, Source, Access,
          alternative_type_alternatives_t<
              std::remove_cv_t<std::remove_reference_t<Source>>>> {};
} // namespace detail
} // namespace dingo
