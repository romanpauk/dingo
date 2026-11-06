//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/base/type_list.h>
#include <dingo/type/access.h>
#include <dingo/type/rebind_type.h>
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

// A conversion is one step of a route from a source to a target: what it
// requires of its source, borrow or consume, and how it is applied. A step that
// continues with a nested conversion applies it to the object it reaches.
// cache_types are the conversions a step retains in its binding.
struct unavailable_type_conversion {
  static constexpr bool available = false;
  using required_access = unavailable_access;
  using cache_types = type_list<>;
};

template <typename Target, typename Source, typename RequiredAccess>
struct conversion_leaf {
  static constexpr bool available = true;

  using required_access = RequiredAccess;
  using cache_types = type_list<>;
};

template <typename Target, typename Source, typename Conversion>
struct conversion_step {
  static constexpr bool available = Conversion::available;

  using required_access = typename Conversion::required_access;
  using cache_types = typename Conversion::cache_types;
};

// The conversion type_conversion_traits declares, called with Argument.
template <typename Target, typename Source, typename RequiredAccess = borrow,
          typename Argument = Source>
struct traits_type_conversion
    : conversion_leaf<Target, Source, RequiredAccess> {
  using argument_type = Argument;

  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &, Context &, Actual &&source, type_descriptor,
                      type_descriptor) {
    using traits = type_conversion_traits<Target, remove_cvref_t<Actual>>;
    if constexpr (std::is_same_v<Argument, Source>) {
      return static_cast<Target>(traits::convert(std::forward<Actual>(source)));
    } else {
      return static_cast<Target>(
          traits::convert(static_cast<Argument>(source)));
    }
  }
};

// The source itself.
template <typename Target, typename Source>
struct identity_type_conversion
    : conversion_leaf<Target, Source,
                      std::conditional_t<std::is_rvalue_reference_v<Source>,
                                         consume, borrow>> {
  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &, Context &, Actual &&source,
                              type_descriptor, type_descriptor) {
    return std::forward<Actual>(source);
  }
};

// A reference to the source, or to a base of it.
template <typename Target, typename Source>
struct reference_type_conversion : conversion_leaf<Target, Source, borrow> {
  template <typename Resolver, typename Context, typename Actual>
  static Target &apply(Resolver &, Context &, Actual &&source, type_descriptor,
                       type_descriptor) {
    return static_cast<Target &>(source);
  }
};

// The address of the source.
template <typename Target, typename Source>
struct address_type_conversion : conversion_leaf<Target, Source, borrow> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &, Context &, Actual &&source, type_descriptor,
                      type_descriptor) {
    return static_cast<Target>(std::addressof(source));
  }
};

// The array a pointer to its first element points to, as a reference.
template <typename Target, typename Source>
struct array_reference_type_conversion
    : conversion_leaf<Target, Source, borrow> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &, Context &, Actual &&source, type_descriptor,
                      type_descriptor) {
    return *reinterpret_cast<
        std::add_pointer_t<std::remove_reference_t<Target>>>(source);
  }
};

// The array a pointer to its first element points to, as a pointer.
template <typename Target, typename Source>
struct array_pointer_type_conversion : conversion_leaf<Target, Source, borrow> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &, Context &, Actual &&source, type_descriptor,
                      type_descriptor) {
    return reinterpret_cast<Target>(source);
  }
};

// The address of the element of a wrapper.
template <typename Target, typename Source>
struct pointer_access_type_conversion
    : conversion_leaf<Target, Source, borrow> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &, Context &, Actual &&source, type_descriptor,
                      type_descriptor) {
    return static_cast<Target>(
        type_traits<remove_cvref_t<Actual>>::get(source));
  }
};

// The pointee of a pointer.
template <typename Target, typename Source, typename Conversion>
struct dereference_type_conversion
    : conversion_step<Target, Source, Conversion> {
  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Actual &&source, type_descriptor requested,
                              type_descriptor registered) {
    return Conversion::apply(resolver, context, *std::forward<Actual>(source),
                             requested, registered);
  }
};

// The element of a wrapper, borrowed.
template <typename Target, typename Source, typename Conversion>
struct borrowed_type_conversion : conversion_step<Target, Source, Conversion> {
  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Actual &&source, type_descriptor requested,
                              type_descriptor registered) {
    return Conversion::apply(resolver, context,
                             type_traits<remove_cvref_t<Actual>>::borrow(
                                 std::forward<Actual>(source)),
                             requested, registered);
  }
};

// An object the binding keeps, converted from the source, that a reference or
// a pointer request refers to.
template <typename Target, typename Source, typename Conversion>
struct retained_type_conversion : conversion_step<Target, Source, Conversion> {
  using retained_type =
      std::remove_cv_t<std::conditional_t<std::is_pointer_v<Target>,
                                          std::remove_pointer_t<Target>,
                                          std::remove_reference_t<Target>>>;
  using cache_types =
      type_list_unique_t<type_list_cat_t<type_list<retained_type>,
                                         typename Conversion::cache_types>>;

  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &resolver, Context &context, Actual &&source,
                      type_descriptor requested, type_descriptor registered) {
    auto &retained = resolver.template retain_conversion<retained_type>(
        context,
        Conversion::apply(resolver, context, std::forward<Actual>(source),
                          requested, registered));
    if constexpr (std::is_pointer_v<Target>) {
      return std::addressof(retained);
    } else {
      return retained;
    }
  }
};

// An optional value made of the converted source, or of the converted element
// of a source optional value.
template <typename Target, typename Source, typename Conversion,
          bool UnwrapSource>
struct wrapper_type_conversion : conversion_step<Target, Source, Conversion> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &resolver, Context &context, Actual &&source,
                      type_descriptor requested, type_descriptor registered) {
    using target = type_traits<remove_cvref_t<Target>>;
    if constexpr (UnwrapSource) {
      using source_traits = type_traits<remove_cvref_t<Actual>>;
      if (source_traits::empty(source)) {
        return target::make_empty();
      }
      auto *value = source_traits::get(source);
      if constexpr (std::is_lvalue_reference_v<Actual>) {
        return target::wrap(Conversion::apply(resolver, context, *value,
                                              requested, registered));
      } else {
        return target::wrap(Conversion::apply(
            resolver, context, std::move(*value), requested, registered));
      }
    } else {
      return target::wrap(Conversion::apply(resolver, context,
                                            std::forward<Actual>(source),
                                            requested, registered));
    }
  }
};

// An alternative type holding the converted source as Selected.
template <typename Target, typename Source, typename Selected,
          typename Conversion>
struct target_alternative_type_conversion
    : conversion_step<Target, Source, Conversion> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &resolver, Context &context, Actual &&source,
                      type_descriptor requested, type_descriptor registered) {
    return type_traits<remove_cvref_t<Target>>::template wrap<Selected>(
        Conversion::apply(resolver, context, std::forward<Actual>(source),
                          requested, registered));
  }
};

// A new owning handle of a consumed value, converted to the element it holds.
// A value of a class derived from the element is held as itself.
template <typename Target, typename Source, typename Held, typename Conversion>
struct boxed_type_conversion : conversion_step<Target, Source, Conversion> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &resolver, Context &context, Actual &&source,
                      type_descriptor requested, type_descriptor registered) {
    using handle =
        typename type_traits<remove_cvref_t<Target>>::template rebind_t<Held>;
    // The value is taken before it is moved into the handle: GCC then folds a
    // handle that does not escape, as it does the value.
    Held value(Conversion::apply(resolver, context,
                                 std::forward<Actual>(source), requested,
                                 registered));
    return Target(type_traits<handle>::make(std::move(value)));
  }
};

// Marks a conversion that constructs a new object; it applies as its
// conversion does.
template <typename Conversion> struct converted_construction : Conversion {};

template <typename Alternative, typename Conversion>
struct alternative_type_conversion_branch {
  using alternative_type = Alternative;
  using conversion = Conversion;
};

template <typename Target, typename Source, typename Branches>
struct alternative_type_conversion;

// The converted alternative a source alternative type holds, tried in order.
template <typename Target, typename Source, typename... Branches>
struct alternative_type_conversion<Target, Source, type_list<Branches...>> {
  static constexpr bool available = (Branches::conversion::available || ...);

  using required_access =
      alternative_access<typename Branches::conversion::required_access...>;
  using cache_types = type_list_unique_t<
      type_list_cat_t<typename Branches::conversion::cache_types...>>;

  // The result of the branch that converts the held alternative is passed
  // through as that branch produces it.
  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Actual &&source, type_descriptor requested,
                              type_descriptor registered) {
    return apply_branch<Branches...>(
        resolver, context, std::forward<Actual>(source), requested, registered);
  }

private:
  template <typename Branch, typename... Rest, typename Resolver,
            typename Context, typename Actual>
  static decltype(auto) apply_branch(Resolver &resolver, Context &context,
                                     Actual &&source, type_descriptor requested,
                                     type_descriptor registered) {
    using conversion = typename Branch::conversion;
    if constexpr (conversion::available) {
      using source_traits = type_traits<remove_cvref_t<Actual>>;
      if (auto *value =
              source_traits::template get<typename Branch::alternative_type>(
                  source)) {
        if constexpr (std::is_lvalue_reference_v<Actual>) {
          return conversion::apply(resolver, context, *value, requested,
                                   registered);
        } else {
          return conversion::apply(resolver, context, std::move(*value),
                                   requested, registered);
        }
      }
    }
    if constexpr ((Rest::conversion::available || ...)) {
      return apply_branch<Rest...>(resolver, context,
                                   std::forward<Actual>(source), requested,
                                   registered);
    } else {
      throw make_type_not_convertible_exception(requested, registered);
    }
  }
};

// The route from Source to Target that Access allows. A pair with a custom
// type_conversion_traits takes only its conversion; any other pair takes the
// first of the default conversions and structural candidates that is
// available.
template <typename Target, typename Source, typename Access = consume,
          bool Default = is_default_type_conversion<
              type_conversion_traits<Target, remove_cvref_t<Source>>>::value>
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
inline constexpr bool is_value_wrapper_type_v = is_optional_type_v<Type>;

template <typename Target>
inline constexpr bool is_structural_conversion_target_v = [] {
  using target_type = remove_cvref_t<Target>;
  return !std::is_reference_v<Target> && !std::is_pointer_v<Target> &&
         (is_alternative_type_v<target_type> ||
          is_value_wrapper_type_v<target_type>);
}();

template <typename Type, typename = void>
struct borrows_value_wrapper : std::false_type {};

template <typename Type>
struct borrows_value_wrapper<
    Type, std::void_t<decltype(type_traits<remove_cvref_t<Type>>::borrow(
              std::declval<Type>()))>>
    : std::bool_constant<is_value_borrowable_v<remove_cvref_t<Type>> &&
                         is_value_wrapper_type_v<
                             decltype(type_traits<remove_cvref_t<Type>>::borrow(
                                 std::declval<Type>()))>> {};

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

// An alternative type built from the source as the alternative overload
// resolution selects.
template <typename Target, typename Source, typename Access,
          bool Enabled = is_structural_conversion_target_v<Target> &&
                         is_alternative_type_v<Target>,
          typename = void>
struct target_alternative_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
struct target_alternative_type_conversion_candidate<
    Target, Source, Access, true,
    std::void_t<typename type_traits<
        std::remove_cv_t<Target>>::template selected_type<Source>>> {
private:
  using selected = typename type_traits<
      std::remove_cv_t<Target>>::template selected_type<Source>;
  using conversion =
      typename type_conversion_path<selected, Source, Access>::type;

public:
  using type = std::conditional_t<
      conversion::available,
      converted_construction<target_alternative_type_conversion<
          Target, Source, selected, conversion>>,
      unavailable_type_conversion>;
};

// The element of an optional source, with the value category of the source.
template <typename Source, bool Unwrap = is_value_wrapper_type_v<Source>>
struct wrapper_element_source {
  using type = Source;
};

template <typename Source> struct wrapper_element_source<Source, true> {
  using type =
      alternative_source_t<typename type_traits<std::remove_cv_t<
                               std::remove_reference_t<Source>>>::value_type,
                           Source>;
};

// An optional value made of the converted source, or of the converted element
// of an optional source.
template <typename Target, typename Source, typename Access,
          bool Enabled = is_structural_conversion_target_v<Target> &&
                         is_value_wrapper_type_v<Target>>
struct target_wrapper_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
struct target_wrapper_type_conversion_candidate<Target, Source, Access, true> {
private:
  static constexpr bool unwrap = is_value_wrapper_type_v<Source>;
  using conversion = typename type_conversion_path<
      typename type_traits<std::remove_cv_t<Target>>::value_type,
      typename wrapper_element_source<Source>::type, Access>::type;

public:
  using type = std::conditional_t<
      (unwrap || !borrows_value_wrapper_v<Source>) && conversion::available,
      compose_wrapper_type_conversion_t<Target, Source, conversion, unwrap>,
      unavailable_type_conversion>;
};

// The converted alternative a source alternative type holds: any of them for
// a reference or pointer target, otherwise the target itself when it is an
// alternative exactly once.
template <typename Target, typename Source, typename Access,
          typename Alternatives>
struct alternative_type_conversion_branches;

template <typename Target, typename Source, typename Access,
          typename... Alternatives>
struct alternative_type_conversion_branches<Target, Source, Access,
                                            type_list<Alternatives...>> {
  using type = alternative_type_conversion<
      Target, Source,
      type_list<alternative_type_conversion_branch<
          Alternatives, typename type_conversion_path<
                            Target, alternative_source_t<Alternatives, Source>,
                            Access>::type>...>>;
};

template <typename Target, typename Source, typename Access,
          bool IsAlternative =
              is_alternative_type_v<remove_cvref_t<Source>> &&
              !std::is_volatile_v<std::remove_reference_t<Source>>>
struct source_alternative_type_conversion {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
struct source_alternative_type_conversion<Target, Source, Access, true> {
private:
  using source_type = remove_cvref_t<Source>;
  using target_type = remove_cvref_t<Target>;
  using alternatives = std::conditional_t<
      std::is_reference_v<Target> || std::is_pointer_v<Target>,
      alternative_type_alternatives_t<source_type>,
      std::conditional_t<
          alternative_type_count<source_type, target_type>::value == 1,
          type_list<target_type>, type_list<>>>;

public:
  using type =
      typename alternative_type_conversion_branches<Target, Source, Access,
                                                    alternatives>::type;
};

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
    std::void_t<decltype(type_traits<remove_cvref_t<Source>>::get(
        std::declval<Source>()))>> {
private:
  using source_type = remove_cvref_t<Source>;
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
        is_value_borrowable_v<remove_cvref_t<Source>>,
        std::void_t<decltype(type_traits<remove_cvref_t<Source>>::borrow(
            std::declval<Source>()))>>> {
private:
  using source_type = remove_cvref_t<Source>;
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

template <typename Conversion, typename Access>
using conversion_for_access_t = std::conditional_t<
    Conversion::available &&
        access_satisfies<Access, typename Conversion::required_access>::value,
    Conversion, unavailable_type_conversion>;

enum class basic_conversion_kind { none, identity, reference, converted };

// The default conversions that keep the source object: a reference to it or to
// its base, a copy of it or of its base, or the object moved from an rvalue.
// Every other conversion, taking the address included, is found by the direct
// path and the fallbacks.
//
// The predicate is a function template rather than a lambda in a member
// initializer: MSVC's pre-C++20 lambda processor fails on those lambdas for
// some instantiations.
template <typename Target, typename Source, typename Access,
          bool UsesDefaultConversion>
constexpr basic_conversion_kind basic_conversion_selected() {
  using kind = basic_conversion_kind;
  using source_type = remove_cvref_t<Source>;
  using source_pointer = std::add_pointer_t<std::remove_reference_t<Source>>;
  constexpr bool borrows = std::is_lvalue_reference_v<Source> &&
                           access_satisfies<Access, borrow>::value;
  constexpr bool defaults = UsesDefaultConversion && !std::is_pointer_v<Target>;
  // A value target of a non-volatile source.
  constexpr bool by_value =
      defaults && !std::is_reference_v<Target> &&
      !std::is_volatile_v<std::remove_reference_t<Source>>;
  if constexpr (defaults && std::is_lvalue_reference_v<Target>) {
    return borrows && std::is_convertible_v<
                          source_pointer,
                          std::add_pointer_t<std::remove_reference_t<Target>>>
               ? kind::converted
               : kind::none;
  } else if constexpr (by_value && borrows &&
                       std::is_same_v<std::remove_cv_t<Target>, source_type>) {
    return is_copy_constructible_v<Target> ? kind::identity : kind::none;
  } else if constexpr (by_value && borrows &&
                       std::is_convertible_v<source_pointer,
                                             std::add_pointer_t<Target>>) {
    return is_copy_constructible_v<Target> ? kind::reference : kind::none;
  } else if constexpr (by_value && std::is_rvalue_reference_v<Source> &&
                       std::is_same_v<std::remove_cv_t<Target>, source_type> &&
                       access_satisfies<Access, consume>::value) {
    return std::is_constructible_v<Target, Source> ? kind::identity
                                                   : kind::none;
  } else {
    return kind::none;
  }
}

// The default conversion that keeps the source object, tried before the
// general search.
template <typename Target, typename Source, typename Access>
using basic_fallback = std::conditional_t<
    basic_conversion_selected<Target, Source, Access, true>() ==
        basic_conversion_kind::identity,
    identity_type_conversion<Target, Source>,
    std::conditional_t<
        basic_conversion_selected<Target, Source, Access, true>() ==
            basic_conversion_kind::reference,
        reference_type_conversion<Target, Source>,
        std::conditional_t<
            basic_conversion_selected<Target, Source, Access, true>() ==
                basic_conversion_kind::converted,
            traits_type_conversion<Target, Source, borrow, Source>,
            unavailable_type_conversion>>>;

// Whether the default conversion borrows its source: it consumes an rvalue
// source, and any source of a target that takes ownership; it borrows any
// other source, an lvalue only when the target can be built from it as a const
// lvalue.
template <typename Target, typename Source, typename Traits,
          bool TakesOwnership>
constexpr bool direct_conversion_borrows() {
  if constexpr (std::is_rvalue_reference_v<Source> || TakesOwnership) {
    return false;
  } else if constexpr (std::is_lvalue_reference_v<Source>) {
    return has_compatible_conversion<
        Target, Traits,
        std::add_lvalue_reference_t<
            std::add_const_t<std::remove_reference_t<Source>>>>::value;
  } else {
    return true;
  }
}

// The conversion the default type_conversion_traits performs. Wrappers and
// alternatives are built by the structural candidates instead.
template <typename Target, typename Source, typename Access,
          bool Structural = is_structural_conversion_target_v<Target>>
struct direct_type_conversion {
private:
  using source_type = remove_cvref_t<Source>;
  using traits = type_conversion_traits<Target, source_type>;
  using const_source = std::add_lvalue_reference_t<
      std::add_const_t<std::remove_reference_t<Source>>>;
  static constexpr bool takes_ownership =
      type_traits<remove_cvref_t<Target>>::is_owning_handle &&
      !type_traits<source_type>::is_owning_handle;
  static constexpr bool borrows =
      direct_conversion_borrows<Target, Source, traits, takes_ownership>();
  using argument =
      std::conditional_t<borrows && std::is_lvalue_reference_v<Source>,
                         const_source, Source>;

public:
  using type = std::conditional_t<
      has_compatible_conversion<Target, traits, argument>::value,
      traits_type_conversion<Target, Source,
                             std::conditional_t<borrows, borrow, consume>,
                             argument>,
      unavailable_type_conversion>;
};

template <typename Target, typename Source, typename Access>
struct direct_type_conversion<Target, Source, Access, true> {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
using direct_fallback =
    typename direct_type_conversion<Target, Source, Access>::type;

// The conversion a custom type_conversion_traits declares, with the access it
// requires.
template <typename Target, typename Source, typename Access>
struct custom_type_conversion {
private:
  using traits = type_conversion_traits<Target, remove_cvref_t<Source>>;
  using required_access =
      typename custom_conversion_access<traits, Source>::type;
  static constexpr bool converts =
      has_compatible_conversion<Target, traits, Source>::value;
  static_assert(!converts || std::is_same_v<required_access, borrow> ||
                    std::is_same_v<required_access, consume>,
                "type_conversion_traits specialization must declare "
                "required_access<Source> as borrow or consume");

public:
  using type = conversion_for_access_t<
      std::conditional_t<
          converts,
          traits_type_conversion<Target, Source, required_access, Source>,
          unavailable_type_conversion>,
      Access>;
};

// The address of an lvalue source, for a pointer target it converts to.
template <typename Target, typename Source, typename Access>
using address_fallback = std::conditional_t<
    std::is_pointer_v<Target> && std::is_lvalue_reference_v<Source> &&
        std::is_convertible_v<
            std::add_pointer_t<std::remove_reference_t<Source>>, Target>,
    address_type_conversion<Target, Source>, unavailable_type_conversion>;

template <typename Target, typename Source, typename Access>
using target_alternative_fallback =
    typename target_alternative_type_conversion_candidate<Target, Source,
                                                          Access>::type;

template <typename Target, typename Source, typename Access>
using source_alternative_fallback =
    typename source_alternative_type_conversion<Target, Source, Access>::type;

// An owning handle of a value that is consumed: a handle of an element, such as
// std::unique_ptr<T>, made of the source moved into a new object. The element
// is an object, not another wrapper. It is not a converted_construction: a
// factory makes a handle in place instead of moving into it what it made. The
// element is an object, not another wrapper. It is not a
// converted_construction: a factory makes a handle in place instead of moving
// into it what it made.
template <typename Target, typename Source, typename Access,
          bool Enabled =
              std::is_same_v<Access, consume> && !std::is_reference_v<Target> &&
              !std::is_lvalue_reference_v<Source> &&
              type_traits<remove_cvref_t<Target>>::is_owning_handle &&
              is_value_borrowable_v<remove_cvref_t<Target>> &&
              std::is_class_v<remove_cvref_t<Source>> &&
              !type_traits<remove_cvref_t<Source>>::enabled>
struct boxed_type_conversion_candidate {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access>
struct boxed_type_conversion_candidate<Target, Source, Access, true> {
private:
  using element = std::remove_cv_t<
      typename type_traits<remove_cvref_t<Target>>::value_type>;
  using source_type = remove_cvref_t<Source>;
  using held = std::conditional_t<std::is_base_of_v<element, source_type>,
                                  source_type, element>;

  template <bool Holdable, typename = void> struct select {
    using type = unavailable_type_conversion;
  };

  template <typename Unused> struct select<true, Unused> {
    using conversion =
        typename type_conversion_path<held, Source, Access>::type;
    using type = std::conditional_t<
        conversion::available,
        boxed_type_conversion<Target, Source, held, conversion>,
        unavailable_type_conversion>;
  };

public:
  using type =
      typename select<is_formable_v<held> && !type_traits<held>::enabled>::type;
};

template <typename Target, typename Source, typename Access>
using boxed_fallback =
    typename boxed_type_conversion_candidate<Target, Source, Access>::type;

template <typename Target, typename Source, typename Access>
using target_wrapper_fallback =
    typename target_wrapper_type_conversion_candidate<Target, Source,
                                                      Access>::type;

template <typename Target, typename Source, typename Access>
using array_fallback =
    typename array_type_conversion_candidate<Target, Source>::type;

template <typename Target, typename Source, typename Access>
using pointer_access_fallback =
    typename pointer_access_type_conversion_candidate<Target, Source>::type;

template <typename Target, typename Source, typename Access>
using dereference_fallback =
    typename dereference_type_conversion_candidate<Target, Source,
                                                   Access>::type;

template <typename Target, typename Source, typename Access>
using borrowed_fallback =
    typename borrowed_type_conversion_candidate<Target, Source, Access>::type;

template <typename Target, typename Source, typename Access>
using retained_fallback =
    typename retained_type_conversion_candidate<Target, Source, Access>::type;

// The first fallback, in order, that is available with the access. A later
// fallback is named only once every earlier one has failed, so its path is
// not instantiated before.
template <typename Target, typename Source, typename Access,
          template <typename, typename, typename> typename... Fallbacks>
struct select_conversion_fallback {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access,
          template <typename, typename, typename> typename Fallback,
          template <typename, typename, typename> typename... Fallbacks>
struct select_conversion_fallback<Target, Source, Access, Fallback,
                                  Fallbacks...> {
private:
  using candidate =
      conversion_for_access_t<Fallback<Target, Source, Access>, Access>;

public:
  using type = typename std::conditional_t<
      candidate::available, std::enable_if<true, candidate>,
      select_conversion_fallback<Target, Source, Access, Fallbacks...>>::type;
};

template <typename Target, typename Source, typename Access, bool Default>
struct type_conversion_path {
  using type = typename select_conversion_fallback<
      Target, Source, Access, basic_fallback, direct_fallback,
      target_alternative_fallback, source_alternative_fallback,
      target_wrapper_fallback, array_fallback, address_fallback,
      pointer_access_fallback, dereference_fallback, borrowed_fallback,
      retained_fallback, boxed_fallback>::type;
  static constexpr bool available = type::available;
};

template <typename Target, typename Source, typename Access>
struct type_conversion_path<Target, Source, Access, false> {
  using type = typename custom_type_conversion<Target, Source, Access>::type;
  static constexpr bool available = type::available;
};

template <typename Target, typename Source, typename Access = consume>
using type_conversion_path_t =
    typename type_conversion_path<Target, Source, Access>::type;

// A target that cannot exist is not probed: probing would instantiate it.
template <typename Target, typename Source, typename Access = consume>
inline constexpr bool is_type_conversion_available_v = [] {
  if constexpr (is_formable_v<Target>) {
    return type_conversion_path<Target, Source, Access>::available;
  } else {
    return false;
  }
}();

} // namespace detail
} // namespace dingo
