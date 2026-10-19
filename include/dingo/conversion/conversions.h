//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/conversion/access.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
template <typename TargetType, typename SourceType, typename = void>
struct type_conversion_traits {
  static constexpr bool is_default = true;

  template <
      typename Source, typename Result = TargetType,
      std::enable_if_t<
          !std::is_array_v<Result> &&
              ((std::is_reference_v<Result> &&
                std::is_lvalue_reference_v<Source &&> &&
                std::is_convertible_v<
                    std::add_pointer_t<std::remove_reference_t<Source>>,
                    std::add_pointer_t<std::remove_reference_t<Result>>>) ||
               (!(std::is_reference_v<Result> &&
                  std::is_lvalue_reference_v<Source &&>) &&
                std::is_constructible_v<Result, Source &&>)),
          int> = 0>
  static Result convert(Source &&source) {
    if constexpr (std::is_reference_v<Result>) {
      return static_cast<Result>(source);
    } else {
      return Result(std::forward<Source>(source));
    }
  }
};

template <typename Target, typename Source>
struct type_conversion_traits<
    Target *, Source *,
    std::enable_if_t<std::is_convertible_v<Source *, Target *>>> {
  static constexpr bool is_default = true;

  static Target *convert(Source *source) {
    return static_cast<Target *>(source);
  }
};

namespace detail {
template <typename Target, typename Traits, typename Source, typename = void>
struct has_compatible_conversion : std::false_type {};

template <typename Target, typename Traits, typename Source>
struct has_compatible_conversion<
    Target, Traits, Source,
    std::void_t<decltype(Traits::convert(std::declval<Source>()))>>
    : std::bool_constant<std::is_constructible_v<
          Target, decltype(Traits::convert(std::declval<Source>()))>> {};

template <typename Traits, typename = void>
struct is_default_type_conversion : std::false_type {};

template <typename Traits>
struct is_default_type_conversion<Traits,
                                  std::void_t<decltype(Traits::is_default)>>
    : std::bool_constant<Traits::is_default> {};

template <typename T, typename = void>
struct has_value_type : std::false_type {};

template <typename T>
struct has_value_type<T, std::void_t<typename type_traits<T>::value_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_value_type_v = has_value_type<T>::value;

struct unavailable_access {};
struct missing_access {};

template <typename... Accesses> struct alternative_access {};

template <typename Supplied, typename Required>
struct access_satisfies : std::false_type {};

template <> struct access_satisfies<borrow, borrow> : std::true_type {};
template <> struct access_satisfies<consume, borrow> : std::true_type {};
template <> struct access_satisfies<consume, consume> : std::true_type {};

template <typename Supplied>
struct access_satisfies<Supplied, unavailable_access> : std::true_type {};

template <typename Supplied, typename... Accesses>
struct access_satisfies<Supplied, alternative_access<Accesses...>>
    : std::bool_constant<(access_satisfies<Supplied, Accesses>::value && ...)> {
};

template <typename Traits, typename Source, typename = void>
struct custom_conversion_access {
  using type = missing_access;
};

template <typename Traits, typename Source>
struct custom_conversion_access<
    Traits, Source,
    std::void_t<typename Traits::template required_access<Source>>> {
  using type = typename Traits::template required_access<Source>;
};

struct unavailable_type_conversion {
  static constexpr bool available = false;
  using required_access = unavailable_access;
};

template <typename Target, typename Source, typename RequiredAccess = borrow,
          typename Argument = Source>
struct traits_type_conversion {
  static constexpr bool available = true;

  using target_type = Target;
  using source_type = Source;
  using required_access = RequiredAccess;
  using argument_type = Argument;
};

template <typename Target, typename Source> struct identity_type_conversion {
  static constexpr bool available = true;

  using target_type = Target;
  using source_type = Source;
  using required_access =
      std::conditional_t<std::is_rvalue_reference_v<Source>, consume, borrow>;
};

template <typename Target, typename Source> struct reference_type_conversion {
  static constexpr bool available = true;

  using target_type = Target;
  using source_type = Source;
  using required_access = borrow;
};

template <typename Target, typename Source> struct address_type_conversion {
  static constexpr bool available = true;

  using target_type = Target;
  using source_type = Source;
  using required_access = borrow;
};

template <typename Target, typename Source>
struct array_reference_type_conversion {
  static constexpr bool available = true;

  using target_type = Target;
  using source_type = Source;
  using required_access = borrow;
};

template <typename Target, typename Source>
struct array_pointer_type_conversion {
  static constexpr bool available = true;

  using target_type = Target;
  using source_type = Source;
  using required_access = borrow;
};

template <typename Target, typename Source>
struct pointer_access_type_conversion {
  static constexpr bool available = true;

  using target_type = Target;
  using source_type = Source;
  using required_access = borrow;
};

template <typename Target, typename Source, typename Conversion>
struct dereference_type_conversion {
  static constexpr bool available = Conversion::available;

  using target_type = Target;
  using source_type = Source;
  using conversion = Conversion;
  using required_access = typename Conversion::required_access;
};

template <typename Target, typename Source, typename Conversion>
struct borrowed_type_conversion {
  static constexpr bool available = Conversion::available;

  using target_type = Target;
  using source_type = Source;
  using conversion = Conversion;
  using required_access = typename Conversion::required_access;
};

template <typename Target, typename Source, typename Conversion>
struct retained_type_conversion {
  static constexpr bool available = Conversion::available;

  using target_type = Target;
  using source_type = Source;
  using conversion = Conversion;
  using required_access = typename Conversion::required_access;
};

template <typename Target, typename Source, typename Conversion,
          bool UnwrapSource>
struct wrapper_type_conversion {
  static constexpr bool available = Conversion::available;

  using target_type = Target;
  using source_type = Source;
  using conversion = Conversion;
  using required_access = typename Conversion::required_access;
  static constexpr bool unwrap_source = UnwrapSource;
};

template <typename Target, typename Source, typename Selected,
          typename Conversion>
struct target_alternative_type_conversion {
  static constexpr bool available = Conversion::available;

  using target_type = Target;
  using source_type = Source;
  using selected_type = Selected;
  using conversion = Conversion;
  using required_access = typename Conversion::required_access;
};

template <typename Conversion> struct converted_construction {
  static constexpr bool available = Conversion::available;

  using conversion = Conversion;
  using required_access = typename Conversion::required_access;
};

template <typename Alternative, typename Conversion>
struct alternative_type_conversion_branch {
  using alternative_type = Alternative;
  using conversion = Conversion;
};

template <typename Target, typename Source, typename Branches>
struct alternative_type_conversion;

template <typename Target, typename Source, typename... Branches>
struct alternative_type_conversion<Target, Source, type_list<Branches...>> {
  static constexpr bool available = (Branches::conversion::available || ...);

  using target_type = Target;
  using source_type = Source;
  using branches = type_list<Branches...>;
  using required_access =
      alternative_access<typename Branches::conversion::required_access...>;
};

template <typename From, typename To> struct copy_cv {
private:
  using const_type =
      std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;

public:
  using type = std::conditional_t<std::is_volatile_v<From>,
                                  std::add_volatile_t<const_type>, const_type>;
};

template <typename From, typename To>
using copy_cv_t = typename copy_cv<From, To>::type;

template <typename... Conversions> struct select_type_conversion;

template <> struct select_type_conversion<> {
  using type = unavailable_type_conversion;
};

template <typename Head, typename... Tail>
struct select_type_conversion<Head, Tail...> {
  using type =
      std::conditional_t<Head::available, Head,
                         typename select_type_conversion<Tail...>::type>;
};

template <typename Conversion, typename Access>
using conversion_for_access_t = std::conditional_t<
    Conversion::available &&
        access_satisfies<Access, typename Conversion::required_access>::value,
    Conversion, unavailable_type_conversion>;

// Keep direct conversions separate from recursive candidates.
template <typename Target, typename Source, typename Access>
struct direct_type_conversion_path;

template <typename Target, typename Source, typename Access>
struct basic_type_conversion_path;

template <typename Target, typename Source, typename Access,
          typename Direct = direct_type_conversion_path<Target, Source, Access>,
          bool UseFallback =
              Direct::uses_default_conversion && !Direct::available>
struct type_conversion_path_impl;

template <typename Target, typename Source, typename Access = consume,
          typename Basic = basic_type_conversion_path<Target, Source, Access>,
          bool UseBasic = Basic::available>
struct type_conversion_path;
} // namespace detail
} // namespace dingo
