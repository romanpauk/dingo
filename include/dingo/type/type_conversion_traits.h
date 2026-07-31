//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/resolution/resolution.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_list.h>
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

template <typename Candidate, typename Next,
          bool Available = Candidate::available>
struct select_conversion_candidate;

template <typename Candidate, typename Next>
struct select_conversion_candidate<Candidate, Next, true> {
  using type = Candidate;
};

template <typename Candidate, typename Next>
struct select_conversion_candidate<Candidate, Next, false> {
  using type = typename Next::type;
};

// Keep the common default cases out of the recursive conversion search. These
// conditions mirror the leading candidates in direct_type_conversion_path.
template <typename Target, typename Source, typename Access>
struct basic_type_conversion_path {
private:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using conversion_traits = type_conversion_traits<Target, source_type>;
  static constexpr bool uses_default_conversion =
      is_default_type_conversion<conversion_traits>::value;
  static constexpr bool can_borrow = access_satisfies<Access, borrow>::value;
  static constexpr bool can_consume = access_satisfies<Access, consume>::value;
  static constexpr bool preserves_identity =
      uses_default_conversion && !std::is_reference_v<Target> &&
      !std::is_pointer_v<Target> &&
      !std::is_volatile_v<std::remove_reference_t<Source>> &&
      std::is_same_v<std::remove_cv_t<Target>, source_type> &&
      ((std::is_lvalue_reference_v<Source> && can_borrow &&
        is_copy_constructible_v<Target>) ||
       (std::is_rvalue_reference_v<Source> && can_consume &&
        std::is_constructible_v<Target, Source>));
  static constexpr bool preserves_reference =
      uses_default_conversion && can_borrow && !std::is_reference_v<Target> &&
      !std::is_pointer_v<Target> && std::is_lvalue_reference_v<Source> &&
      !std::is_volatile_v<std::remove_reference_t<Source>> &&
      is_copy_constructible_v<Target> &&
      std::is_convertible_v<std::add_pointer_t<std::remove_reference_t<Source>>,
                            std::add_pointer_t<Target>>;
  static constexpr bool converts_reference =
      uses_default_conversion && can_borrow &&
      std::is_lvalue_reference_v<Target> &&
      std::is_lvalue_reference_v<Source> &&
      std::is_convertible_v<
          std::add_pointer_t<std::remove_reference_t<Source>>,
          std::add_pointer_t<std::remove_reference_t<Target>>>;

public:
  using type = std::conditional_t<
      preserves_identity, identity_type_conversion<Target, Source>,
      std::conditional_t<
          preserves_reference, reference_type_conversion<Target, Source>,
          std::conditional_t<
              converts_reference,
              traits_type_conversion<Target, Source, borrow, Source>,
              unavailable_type_conversion>>>;
  static constexpr bool available = type::available;
};

template <typename Target, typename Source, typename Access>
struct direct_type_conversion_path {
private:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using conversion_traits = type_conversion_traits<Target, source_type>;

public:
  static constexpr bool uses_default_conversion =
      is_default_type_conversion<conversion_traits>::value;

private:
  static constexpr bool borrows_existing_object =
      std::is_lvalue_reference_v<Target> &&
      std::is_lvalue_reference_v<Source> &&
      std::is_convertible_v<
          std::add_pointer_t<std::remove_reference_t<Source>>,
          std::add_pointer_t<std::remove_reference_t<Target>>>;
  static constexpr bool conversion_takes_ownership =
      !borrows_existing_object && type_traits<target_type>::is_owning_handle &&
      !type_traits<source_type>::is_owning_handle;
  using const_source = std::add_lvalue_reference_t<
      std::add_const_t<std::remove_reference_t<Source>>>;
  static constexpr bool has_const_conversion = [] {
    if constexpr (uses_default_conversion &&
                  std::is_lvalue_reference_v<Source>) {
      return has_compatible_conversion<Target, conversion_traits,
                                       const_source>::value;
    } else {
      return false;
    }
  }();
  using default_required_access = std::conditional_t<
      std::is_rvalue_reference_v<Source> || conversion_takes_ownership ||
          (std::is_lvalue_reference_v<Source> && !borrows_existing_object &&
           !has_const_conversion),
      consume, borrow>;
  using custom_required_access =
      typename custom_conversion_access<conversion_traits, Source>::type;
  using required_access =
      std::conditional_t<uses_default_conversion, default_required_access,
                         custom_required_access>;
  using conversion_source =
      std::conditional_t<uses_default_conversion &&
                             std::is_same_v<default_required_access, borrow> &&
                             std::is_lvalue_reference_v<Source> &&
                             !borrows_existing_object,
                         const_source, Source>;
  static constexpr bool has_traits_conversion =
      has_compatible_conversion<Target, conversion_traits,
                                conversion_source>::value;
  static constexpr bool has_valid_custom_access =
      std::is_same_v<custom_required_access, borrow> ||
      std::is_same_v<custom_required_access, consume>;
  static_assert(uses_default_conversion || !has_traits_conversion ||
                    has_valid_custom_access,
                "type_conversion_traits specialization must declare "
                "required_access<Source> as borrow or consume");
  static constexpr bool preserves_identity =
      uses_default_conversion && !std::is_reference_v<Target> &&
      !std::is_pointer_v<Target> &&
      !std::is_volatile_v<std::remove_reference_t<Source>> &&
      std::is_same_v<std::remove_cv_t<Target>, source_type> &&
      ((std::is_lvalue_reference_v<Source> &&
        is_copy_constructible_v<Target>) ||
       (std::is_rvalue_reference_v<Source> &&
        std::is_constructible_v<Target, Source>));
  static constexpr bool takes_address =
      std::is_pointer_v<Target> && std::is_lvalue_reference_v<Source> &&
      std::is_convertible_v<std::add_pointer_t<std::remove_reference_t<Source>>,
                            Target>;
  static constexpr bool preserves_reference =
      !std::is_reference_v<Target> && !std::is_pointer_v<Target> &&
      uses_default_conversion && std::is_lvalue_reference_v<Source> &&
      !std::is_volatile_v<std::remove_reference_t<Source>> &&
      is_copy_constructible_v<Target> &&
      std::is_convertible_v<std::add_pointer_t<std::remove_reference_t<Source>>,
                            std::add_pointer_t<Target>>;

  using identity = std::conditional_t<preserves_identity,
                                      identity_type_conversion<Target, Source>,
                                      unavailable_type_conversion>;
  using reference =
      std::conditional_t<preserves_reference,
                         reference_type_conversion<Target, Source>,
                         unavailable_type_conversion>;
  using direct =
      std::conditional_t<has_traits_conversion &&
                             (!uses_default_conversion ||
                              !is_structural_conversion_target_v<Target>),
                         traits_type_conversion<Target, Source, required_access,
                                                conversion_source>,
                         unavailable_type_conversion>;
  using selected = typename select_type_conversion<
      conversion_for_access_t<identity, Access>,
      conversion_for_access_t<reference, Access>,
      conversion_for_access_t<direct, Access>>::type;

public:
  static constexpr bool can_take_address = takes_address;
  using type = selected;
  static constexpr bool available = type::available;
};

template <std::size_t Index, typename Target, typename Source, typename Access,
          typename Direct>
struct conversion_fallback_candidate;

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<0, Target, Source, Access, Direct> {
  using type =
      typename target_alternative_type_conversion_candidate<Target, Source,
                                                            Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<1, Target, Source, Access, Direct> {
  using type =
      typename source_alternative_type_conversion<Target, Source, Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<2, Target, Source, Access, Direct> {
  using type = typename target_wrapper_type_conversion_candidate<Target, Source,
                                                                 Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<3, Target, Source, Access, Direct> {
  using type = typename array_type_conversion_candidate<Target, Source>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<4, Target, Source, Access, Direct> {
  using type = std::conditional_t<Direct::can_take_address,
                                  address_type_conversion<Target, Source>,
                                  unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<5, Target, Source, Access, Direct> {
  using type =
      typename pointer_access_type_conversion_candidate<Target, Source>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<6, Target, Source, Access, Direct> {
  using type = typename dereference_type_conversion_candidate<Target, Source,
                                                              Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<7, Target, Source, Access, Direct> {
  using type =
      typename borrowed_type_conversion_candidate<Target, Source, Access>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct conversion_fallback_candidate<8, Target, Source, Access, Direct> {
  using type =
      typename retained_type_conversion_candidate<Target, Source, Access>::type;
};

template <typename Target, typename Source, typename Direct>
inline constexpr std::size_t first_conversion_fallback_v = [] {
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
  using target_array = std::conditional_t<std::is_pointer_v<Target>,
                                          std::remove_pointer_t<Target>,
                                          std::remove_reference_t<Target>>;

  // These are necessary conditions only. The selected candidate still
  // performs its complete validity check before it can win.
  if constexpr (!std::is_reference_v<Target> && !std::is_pointer_v<Target> &&
                is_alternative_type_v<target_type>) {
    return 0;
  } else if constexpr (is_alternative_type_v<source_type> &&
                       !std::is_volatile_v<std::remove_reference_t<Source>>) {
    return 1;
  } else if constexpr (!std::is_reference_v<Target> &&
                       !std::is_pointer_v<Target> &&
                       is_value_wrapper_type_v<target_type>) {
    return 2;
  } else if constexpr (std::is_pointer_v<std::remove_reference_t<Source>> &&
                       ((std::is_lvalue_reference_v<Target> &&
                         std::is_array_v<std::remove_reference_t<Target>>) ||
                        (std::is_pointer_v<Target> &&
                         std::is_array_v<target_array>))) {
    return 3;
  } else if constexpr (Direct::can_take_address) {
    return 4;
  } else if constexpr (std::is_pointer_v<Target> &&
                       std::is_lvalue_reference_v<Source>) {
    return 5;
  } else if constexpr (std::is_pointer_v<std::remove_reference_t<Source>> &&
                       !std::is_void_v<std::remove_pointer_t<
                           std::remove_reference_t<Source>>> &&
                       !std::is_pointer_v<Target>) {
    return 6;
  } else if constexpr (type_traits<source_type>::is_value_borrowable) {
    return 7;
  } else if constexpr ((std::is_lvalue_reference_v<Target> ||
                        std::is_pointer_v<Target>) &&
                       type_traits<
                           retained_conversion_type_t<Target>>::enabled &&
                       !std::is_array_v<retained_conversion_type_t<Target>>) {
    return 8;
  } else {
    return 9;
  }
}();

// Keep later recursive candidates incomplete until every earlier conversion
// has failed; merely naming all candidates instantiates all of their paths.
template <std::size_t Index, typename Target, typename Source, typename Access,
          typename Direct>
struct select_conversion_fallback {
private:
  using candidate =
      typename conversion_fallback_candidate<Index, Target, Source, Access,
                                             Direct>::type;
  using accessible_candidate = conversion_for_access_t<candidate, Access>;

public:
  using type = typename select_conversion_candidate<
      accessible_candidate,
      select_conversion_fallback<Index + 1, Target, Source, Access,
                                 Direct>>::type;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct select_conversion_fallback<9, Target, Source, Access, Direct> {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct type_conversion_path_impl<Target, Source, Access, Direct, true> {
public:
  using type = typename select_conversion_fallback<
      first_conversion_fallback_v<Target, Source, Direct>, Target, Source,
      Access, Direct>::type;
  static constexpr bool available = type::available;
};

template <typename Target, typename Source, typename Access, typename Direct>
struct type_conversion_path_impl<Target, Source, Access, Direct, false>
    : Direct {};

template <typename Target, typename Source, typename Access, typename Basic>
struct type_conversion_path<Target, Source, Access, Basic, true> : Basic {};

template <typename Target, typename Source, typename Access, typename Basic>
struct type_conversion_path<Target, Source, Access, Basic, false>
    : type_conversion_path_impl<
          Target, Source, Access,
          conversion_direct_path_t<Target, Source, Access>> {};

template <typename Target, typename Source, typename Access = consume>
using type_conversion_path_t =
    typename type_conversion_path<Target, Source, Access>::type;

template <typename Target, typename Source, typename Access = consume>
inline constexpr bool is_type_conversion_available_v =
    type_conversion_path<Target, Source, Access>::available;

template <typename Target, typename Source, typename Access = consume>
struct is_type_conversion_available
    : std::bool_constant<
          is_type_conversion_available_v<Target, Source, Access>> {};

template <typename Target>
using conversion_target_t =
    std::conditional_t<std::is_rvalue_reference_v<Target>,
                       std::remove_reference_t<Target>, Target>;

template <typename Target, typename Source, typename Access = consume>
using conversion_resolution = resolution<
    Target, type_resolution<conversion_target_t<Target>, Source,
                            type_conversion_path_t<conversion_target_t<Target>,
                                                   Source, Access>>>;
} // namespace detail

template <typename Source, typename Interface, typename Access, typename = void>
struct wrapper_resolution_traits {
  using type = type_list<>;
};

template <typename Source, typename Interface, typename Access>
struct wrapper_resolution_traits<
    Source, Interface, Access,
    std::enable_if_t<
        type_traits<std::remove_pointer_t<
            std::remove_cv_t<std::remove_reference_t<Source>>>>::enabled &&
        !type_traits<std::remove_cv_t<Interface>>::enabled &&
        (std::is_same_v<Access, borrow> || std::is_same_v<Access, consume>)>> {
private:
  using source_type =
      std::remove_pointer_t<std::remove_cv_t<std::remove_reference_t<Source>>>;
  using target_type = detail::wrapper_rebind_leaf_t<source_type, Interface>;

public:
  using type = std::conditional_t<
      !std::is_same_v<target_type, source_type> &&
          detail::is_type_conversion_available_v<target_type, Source, Access>,
      type_list<detail::conversion_resolution<target_type, Source, Access>>,
      type_list<>>;
};
} // namespace dingo
