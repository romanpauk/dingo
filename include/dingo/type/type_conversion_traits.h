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
#include <dingo/type/shape.h>
#include <dingo/type/type_traits.h>

#include <type_traits>
#include <utility>

namespace dingo {
// How a conversion accesses its source: it borrows it, or it consumes it.
struct borrow {};
struct consume {};

// Whether a value of Target made of Source would slice it: Target is a base
// of Source, other than Source itself. A base is given by reference, by
// pointer or by a handle holding the object, never as a copy of it.
template <typename Target, typename Source>
inline constexpr bool slices_v =
    !std::is_reference_v<Target> &&
    std::is_base_of_v<std::remove_cv_t<Target>,
                      std::remove_cv_t<std::remove_reference_t<Source>>> &&
    !std::is_same_v<std::remove_cv_t<Target>,
                    std::remove_cv_t<std::remove_reference_t<Source>>>;

template <typename TargetType, typename SourceType, typename = void>
struct type_conversion_traits {
  static constexpr bool is_default = true;

  template <
      typename Source, typename Result = TargetType,
      std::enable_if_t<
          !std::is_array_v<Result> && !slices_v<Result, Source> &&
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

// A conversion converts a source to a target, as a path of operations. An
// operation is a class with a static apply: applied to the source
// (conversion_leaf), to unwrap it before the inner conversion
// (conversion_unwrap), or to wrap what the inner conversion returns
// (conversion_wrap). The paths of the operations are tried in the order
// of op, and the first available one is taken.
//
// A conversion says whether it is available, the access it requires of its
// source, and how it applies.
struct unavailable_type_conversion {
  static constexpr bool available = false;
  using required_access = unavailable_access;
};

// Op applied to the source.
template <typename Op, typename Target, typename RequiredAccess>
struct conversion_leaf {
  static constexpr bool available = true;
  using required_access = RequiredAccess;

  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &, Context &, Actual &&source,
                              type_descriptor, type_descriptor) {
    return Op::template apply<Target>(std::forward<Actual>(source));
  }
};

// The availability and access of an inner conversion.
template <typename Inner> struct conversion_inner {
  static constexpr bool available = Inner::available;
  using required_access = typename Inner::required_access;
};

// Inner applied to what Op unwraps of the source.
template <typename Op, typename Inner>
struct conversion_unwrap : conversion_inner<Inner> {
  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Actual &&source, type_descriptor requested,
                              type_descriptor registered) {
    return Inner::apply(resolver, context,
                        Op::apply(std::forward<Actual>(source)), requested,
                        registered);
  }
};

// Op wrapping what Inner returns for the source. Op is given a function that
// applies Inner, so that a value Inner returns initializes what Op makes
// directly.
template <typename Op, typename Target, typename Inner>
struct conversion_wrap : conversion_inner<Inner> {

  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &resolver, Context &context, Actual &&source,
                      type_descriptor requested, type_descriptor registered) {
    return Op::template apply<Target>(
        resolver, context, [&]() -> decltype(auto) {
          return Inner::apply(resolver, context, std::forward<Actual>(source),
                              requested, registered);
        });
  }
};

// Marks a conversion that constructs a new object; it applies as Conversion.
template <typename Conversion> struct converted_construction : Conversion {};

// Conversion, when Inner is available.
template <typename Conversion, typename Inner>
using available_t = std::conditional_t<Inner::available, Conversion,
                                       unavailable_type_conversion>;

// The conversion from Source to Target that Access allows. A pair with a
// custom type_conversion_traits takes only its conversion; any other pair takes
// the first available path of the operations.
template <typename Target, typename Source, typename Access = consume,
          bool Default = is_default_type_conversion<
              type_conversion_traits<Target, remove_cvref_t<Source>>>::value>
struct type_conversion_path;

template <typename Target, typename Source, typename Access>
using path_t = typename type_conversion_path<Target, Source, Access>::type;

template <typename Alternative, typename Source>
using qualified_alternative_t =
    copy_cv_t<std::remove_reference_t<Source>, Alternative>;

// An alternative of an alternative type, with the value category of the
// alternative type.
template <typename Alternative, typename Source>
using alternative_source_t =
    std::conditional_t<std::is_lvalue_reference_v<Source>,
                       qualified_alternative_t<Alternative, Source> &,
                       qualified_alternative_t<Alternative, Source> &&>;

template <typename Type>
inline constexpr bool is_value_wrapper_type_v = is_optional_type_v<Type>;

// A target an operation makes rather than converts to: an alternative type or
// an optional value.
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

template <typename Conversion, typename Access>
using conversion_for_access_t = std::conditional_t<
    Conversion::available &&
        access_satisfies<Access, typename Conversion::required_access>::value,
    Conversion, unavailable_type_conversion>;

// --- The operations.

// The source itself.
struct identity {
  template <typename Target, typename Actual>
  static decltype(auto) apply(Actual &&source) {
    return std::forward<Actual>(source);
  }
};

// A reference to the source, or to a base of it.
struct base_ref {
  template <typename Target, typename Actual>
  static Target &apply(Actual &&source) {
    return static_cast<Target &>(source);
  }
};

// What type_conversion_traits converts the source to, given as Argument.
template <typename Source, typename Argument> struct convert {
  template <typename Target, typename Actual>
  static Target apply(Actual &&source) {
    using traits = type_conversion_traits<Target, remove_cvref_t<Actual>>;
    if constexpr (std::is_same_v<Argument, Source>) {
      return static_cast<Target>(traits::convert(std::forward<Actual>(source)));
    } else {
      return static_cast<Target>(
          traits::convert(static_cast<Argument>(source)));
    }
  }
};

template <typename Target, typename Source, typename RequiredAccess = borrow,
          typename Argument = Source>
using convert_t =
    conversion_leaf<convert<Source, Argument>, Target, RequiredAccess>;

// The address of the source.
struct address {
  template <typename Target, typename Actual>
  static Target apply(Actual &&source) {
    return static_cast<Target>(std::addressof(source));
  }
};

// The first element of an array, as the pointer the array decays to.
struct decay {
  template <typename Target, typename Actual>
  static Target apply(Actual &&source) {
    return static_cast<Target>(source);
  }
};

// The array an owning pointer to its first element points to, as a pointer:
// a scope hands an array it makes over as a pointer to its first element.
struct array_view {
  template <typename Target, typename Actual>
  static Target apply(Actual &&source) {
    return reinterpret_cast<Target>(source);
  }
};

// The address of the element of a wrapper.
struct element_ptr {
  template <typename Target, typename Actual>
  static Target apply(Actual &&source) {
    return static_cast<Target>(
        type_traits<remove_cvref_t<Actual>>::get(source));
  }
};

// The element of a wrapper, borrowed.
struct borrow_element {
  template <typename Actual> static decltype(auto) apply(Actual &&source) {
    return type_traits<remove_cvref_t<Actual>>::borrow(
        std::forward<Actual>(source));
  }
};

template <typename Alternative, typename Conversion>
struct alternative_type_conversion_branch {
  using alternative_type = Alternative;
  using conversion = Conversion;
};

template <typename Target, typename Source, typename Branches>
struct alternative_type_conversion;

// The alternative a source alternative type holds: a branch per alternative,
// tried in order, the one held taken at run time.
template <typename Target, typename Source, typename... Branches>
struct alternative_type_conversion<Target, Source, type_list<Branches...>> {
  static constexpr bool available = (Branches::conversion::available || ...);

  using required_access =
      alternative_access<typename Branches::conversion::required_access...>;

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
      throw_type_not_convertible(requested, registered);
    }
  }
};

// The branches of the alternatives of a source alternative type.
template <typename Target, typename Source, typename Access,
          typename... Alternatives>
constexpr auto held_alternative_branches(type_list<Alternatives...>) {
  return alternative_type_conversion<
      Target, Source,
      type_list<alternative_type_conversion_branch<
          Alternatives,
          path_t<Target, alternative_source_t<Alternatives, Source>,
                 Access>>...>>{};
}

// An alternative type holding the converted source as the alternative
// overload resolution selects.
template <typename Selected> struct wrap_alternative {
  template <typename Target, typename Resolver, typename Context,
            typename Inner>
  static Target apply(Resolver &, Context &, Inner &&inner) {
    return type_traits<remove_cvref_t<Target>>::template wrap<Selected>(
        inner());
  }
};

// An optional value made of the converted source.
struct wrap_optional {
  template <typename Target, typename Resolver, typename Context,
            typename Inner>
  static Target apply(Resolver &, Context &, Inner &&inner) {
    return type_traits<remove_cvref_t<Target>>::wrap(inner());
  }
};

// The element of a source optional value that is exactly the target, or an
// empty one when the source is empty. A borrowed source gives its element, or
// the one empty value of the type, to be copied; a consumed source, which the
// request owns, is given an empty element first, and its element is moved
// out. The element is not made anew of what it holds.
struct element_or_empty {
  template <typename Target, typename Source>
  static std::conditional_t<std::is_lvalue_reference_v<Source>,
                            const remove_cvref_t<Target> &,
                            remove_cvref_t<Target> &&>
  apply(Source &&source) {
    using source_traits = type_traits<remove_cvref_t<Source>>;
    using element = remove_cvref_t<Target>;
    if constexpr (std::is_lvalue_reference_v<Source>) {
      if (source_traits::empty(source)) {
        static const element empty = type_traits<element>::make_empty();
        return empty;
      }
      return *source_traits::get(source);
    } else {
      if (source_traits::empty(source)) {
        source = source_traits::wrap(type_traits<element>::make_empty());
      }
      return std::move(*source_traits::get(source));
    }
  }
};

// An optional value made of the converted element of a source optional value,
// empty when the source is.
template <typename Target, typename Inner>
struct optional_element_conversion : conversion_inner<Inner> {
  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &resolver, Context &context, Actual &&source,
                      type_descriptor requested, type_descriptor registered) {
    using target = type_traits<remove_cvref_t<Target>>;
    using source_traits = type_traits<remove_cvref_t<Actual>>;
    if (source_traits::empty(source)) {
      return target::make_empty();
    }
    auto *value = source_traits::get(source);
    if constexpr (std::is_lvalue_reference_v<Actual>) {
      return target::wrap(
          Inner::apply(resolver, context, *value, requested, registered));
    } else {
      return target::wrap(Inner::apply(resolver, context, std::move(*value),
                                       requested, registered));
    }
  }
};

// The optional value made of what Inner returns, a converted_construction when
// Inner is one.
template <typename Target, typename Inner, bool OfElement>
struct optional_conversion {
  using type =
      std::conditional_t<OfElement, optional_element_conversion<Target, Inner>,
                         conversion_wrap<wrap_optional, Target, Inner>>;
};

template <typename Target, typename Inner, bool OfElement>
struct optional_conversion<Target, converted_construction<Inner>, OfElement> {
  using type = converted_construction<
      typename optional_conversion<Target, Inner, OfElement>::type>;
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

// A new owning handle of a consumed value, converted to the element it holds.
// It is not a converted_construction: a factory makes a handle in place
// instead of moving into it what it made.
template <typename Held> struct box {
  template <typename Target, typename Resolver, typename Context,
            typename Inner>
  static Target apply(Resolver &, Context &, Inner &&inner) {
    using handle =
        typename type_traits<remove_cvref_t<Target>>::template rebind_t<Held>;
    if constexpr (std::is_reference_v<decltype(inner())> &&
                  !std::is_trivially_move_constructible_v<Held>) {
      // A value the source holds is moved into the handle once.
      return Target(type_traits<handle>::make(inner()));
    } else {
      // The value is taken before it is moved into the handle: GCC then folds
      // a handle that does not escape, as it does the value. Moving a value
      // that is moved trivially twice costs nothing.
      Held held(inner());
      return Target(type_traits<handle>::make(std::move(held)));
    }
  }
};

// --- The paths of the operations.

enum class basic_conversion_kind { none, identity, reference };

// The default conversions that keep the source object: a reference to it or to
// its base, a copy of it, or the object moved from an rvalue. A copy of its
// base would slice it.
// Every other conversion, taking the address included, is found by the paths
// after it.
template <typename Target, typename Source, typename Access>
constexpr basic_conversion_kind basic_conversion_selected() {
  using kind = basic_conversion_kind;
  using source_type = remove_cvref_t<Source>;
  using source_pointer = std::add_pointer_t<std::remove_reference_t<Source>>;
  constexpr bool borrows = std::is_lvalue_reference_v<Source> &&
                           access_satisfies<Access, borrow>::value;
  constexpr bool defaults = !std::is_pointer_v<Target>;
  // A value target of a non-volatile source.
  constexpr bool by_value =
      defaults && !std::is_reference_v<Target> &&
      !std::is_volatile_v<std::remove_reference_t<Source>>;
  if constexpr (defaults && std::is_lvalue_reference_v<Target>) {
    return borrows && std::is_convertible_v<
                          source_pointer,
                          std::add_pointer_t<std::remove_reference_t<Target>>>
               ? kind::reference
               : kind::none;
  } else if constexpr (by_value && borrows &&
                       std::is_same_v<std::remove_cv_t<Target>, source_type>) {
    return is_copy_constructible_v<Target> ? kind::identity : kind::none;
  } else if constexpr (by_value && std::is_rvalue_reference_v<Source> &&
                       std::is_same_v<std::remove_cv_t<Target>, source_type> &&
                       access_satisfies<Access, consume>::value) {
    return std::is_constructible_v<Target, Source> ? kind::identity
                                                   : kind::none;
  } else {
    return kind::none;
  }
}

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

template <typename Type, typename = void>
inline constexpr bool has_element_ptr_v = false;

template <typename Type>
inline constexpr bool has_element_ptr_v<
    Type, std::void_t<decltype(type_traits<remove_cvref_t<Type>>::get(
              std::declval<Type>()))>> = true;

template <typename Type, typename = void>
inline constexpr bool has_borrow_element_v = false;

template <typename Type>
inline constexpr bool has_borrow_element_v<
    Type, std::void_t<decltype(type_traits<remove_cvref_t<Type>>::borrow(
              std::declval<Type>()))>> =
    is_value_borrowable_v<remove_cvref_t<Type>>;

template <typename Target, typename Source, typename = void>
inline constexpr bool has_selected_alternative_v = false;

template <typename Target, typename Source>
inline constexpr bool has_selected_alternative_v<
    Target, Source,
    std::void_t<typename type_traits<
        std::remove_cv_t<Target>>::template selected_type<Source>>> = true;

// The path of each operation: the conversion it makes of the source with the
// conversion of what it unwraps or wraps, or unavailable when the operation
// does not apply. A path whose inner conversion is unavailable is unavailable.

// The source kept: itself, a reference to it or to its base, or a copy or a
// move of it, tried before the general search.
template <typename Target, typename Source, typename Access>
constexpr auto basic_path() {
  constexpr auto kind = basic_conversion_selected<Target, Source, Access>();
  if constexpr (kind == basic_conversion_kind::identity) {
    return conversion_leaf<
        identity, Target,
        std::conditional_t<std::is_rvalue_reference_v<Source>, consume,
                           borrow>>{};
  } else if constexpr (kind == basic_conversion_kind::reference) {
    return conversion_leaf<base_ref, Target, borrow>{};
  } else {
    return unavailable_type_conversion{};
  }
}

// What the default type_conversion_traits converts the source to. Wrappers and
// alternatives are made by the operations after it instead.
template <typename Target, typename Source, typename Access>
constexpr auto direct_path() {
  if constexpr (is_structural_conversion_target_v<Target>) {
    return unavailable_type_conversion{};
  } else {
    using source_type = remove_cvref_t<Source>;
    using traits = type_conversion_traits<Target, source_type>;
    constexpr bool takes_ownership =
        type_traits<remove_cvref_t<Target>>::is_owning_handle &&
        !type_traits<source_type>::is_owning_handle;
    constexpr bool borrows =
        direct_conversion_borrows<Target, Source, traits, takes_ownership>();
    using argument =
        std::conditional_t<borrows && std::is_lvalue_reference_v<Source>,
                           std::add_lvalue_reference_t<std::add_const_t<
                               std::remove_reference_t<Source>>>,
                           Source>;
    if constexpr (has_compatible_conversion<Target, traits, argument>::value) {
      return convert_t<Target, Source,
                       std::conditional_t<borrows, borrow, consume>,
                       argument>{};
    } else {
      return unavailable_type_conversion{};
    }
  }
}

// An alternative type holding the source as the alternative overload
// resolution selects.
template <typename Target, typename Source, typename Access>
constexpr auto wrap_alternative_path() {
  if constexpr (is_structural_conversion_target_v<Target> &&
                is_alternative_type_v<Target> &&
                has_selected_alternative_v<Target, Source>) {
    using selected = typename type_traits<
        std::remove_cv_t<Target>>::template selected_type<Source>;
    using inner = path_t<selected, Source, Access>;
    return available_t<converted_construction<conversion_wrap<
                           wrap_alternative<selected>, Target, inner>>,
                       inner>{};
  } else {
    return unavailable_type_conversion{};
  }
}

// The alternative a source alternative type holds: any of them for a
// reference or pointer target, otherwise the target itself when it is an
// alternative exactly once.
template <typename Target, typename Source, typename Access>
constexpr auto held_alternative_path() {
  using source_type = remove_cvref_t<Source>;
  if constexpr (is_alternative_type_v<source_type> &&
                !std::is_volatile_v<std::remove_reference_t<Source>>) {
    using target_type = remove_cvref_t<Target>;
    using alternatives = std::conditional_t<
        std::is_reference_v<Target> || std::is_pointer_v<Target>,
        alternative_type_alternatives_t<source_type>,
        std::conditional_t<
            alternative_type_count<source_type, target_type>::value == 1,
            type_list<target_type>, type_list<>>>;
    return held_alternative_branches<Target, Source, Access>(alternatives{});
  } else {
    return unavailable_type_conversion{};
  }
}

// Whether an optional value holds exactly the source, rather than what the
// source borrows.
template <typename Target, typename Source>
inline constexpr bool wraps_exactly_v = [] {
  if constexpr (is_value_wrapper_type_v<remove_cvref_t<Target>>) {
    return std::is_same_v<std::remove_cv_t<typename type_traits<
                              remove_cvref_t<Target>>::value_type>,
                          remove_cvref_t<Source>>;
  } else {
    return false;
  }
}();

// The element of a source optional value that is exactly an optional target,
// given as it is: copied from a borrowed source, moved out of a consumed one.
// Whether a target is exactly the element of a source optional value, as an
// optional value or an rvalue reference to one.
template <typename Target, typename Source> constexpr bool is_exact_element() {
  using source_type = remove_cvref_t<Source>;
  using target_type = remove_cvref_t<Target>;
  if constexpr (std::is_lvalue_reference_v<Target> ||
                std::is_pointer_v<Target> ||
                !is_value_wrapper_type_v<source_type> ||
                !is_value_wrapper_type_v<target_type>) {
    return false;
  } else {
    return std::is_same_v<
        std::remove_cv_t<typename type_traits<source_type>::value_type>,
        target_type>;
  }
}

template <typename Target, typename Source, typename Access>
constexpr auto element_or_empty_path() {
  constexpr bool exact = is_exact_element<Target, Source>();
  constexpr bool borrows = exact && std::is_lvalue_reference_v<Source> &&
                           !std::is_rvalue_reference_v<Target> &&
                           is_copy_constructible_v<remove_cvref_t<Target>>;
  constexpr bool consumes =
      exact && !std::is_lvalue_reference_v<Source> &&
      access_satisfies<Access, consume>::value &&
      std::is_move_constructible_v<remove_cvref_t<Target>> &&
      std::is_move_assignable_v<remove_cvref_t<Source>>;
  if constexpr (borrows) {
    return conversion_leaf<element_or_empty, Target, borrow>{};
  } else if constexpr (consumes) {
    return conversion_leaf<element_or_empty, Target, consume>{};
  } else {
    return unavailable_type_conversion{};
  }
}

// An optional value made of the source, or of the element of a source
// optional value. A source that borrows an optional value is held as itself
// only by an optional of exactly it.
template <typename Target, typename Source, typename Access>
constexpr auto wrap_optional_path() {
  constexpr bool of_element = is_value_wrapper_type_v<Source>;
  if constexpr (is_structural_conversion_target_v<Target> &&
                is_value_wrapper_type_v<Target> &&
                (of_element || !borrows_value_wrapper_v<Source> ||
                 wraps_exactly_v<Target, Source>)) {
    using inner =
        path_t<typename type_traits<std::remove_cv_t<Target>>::value_type,
               typename wrapper_element_source<Source>::type, Access>;
    return available_t<
        typename optional_conversion<Target, inner, of_element>::type, inner>{};
  } else {
    return unavailable_type_conversion{};
  }
}

// The pointer an lvalue array decays to, for a pointer target it converts to.
template <typename Target, typename Source, typename Access>
constexpr auto decay_path() {
  using array = std::remove_reference_t<Source>;
  if constexpr (std::is_pointer_v<Target> &&
                std::is_lvalue_reference_v<Source> && std::is_array_v<array> &&
                std::is_convertible_v<
                    std::add_pointer_t<std::remove_extent_t<array>>, Target>) {
    return conversion_leaf<decay, Target, borrow>{};
  } else {
    return unavailable_type_conversion{};
  }
}

// A pointer to the array a pointer to its first element points to.
template <typename Array, typename Element> constexpr bool views_array() {
  if constexpr (std::is_array_v<Array>) {
    using row = std::remove_extent_t<Array>;
    return std::is_same_v<std::remove_cv_t<Element>, std::remove_cv_t<row>> &&
           std::is_convertible_v<Element *, row *>;
  } else {
    return false;
  }
}

template <typename Target, typename Source, typename Access>
constexpr auto array_view_path() {
  using source = std::remove_reference_t<Source>;
  if constexpr (std::is_pointer_v<Target> && std::is_pointer_v<source>) {
    if constexpr (views_array<std::remove_pointer_t<Target>,
                              std::remove_pointer_t<source>>()) {
      return conversion_leaf<array_view, Target, borrow>{};
    } else {
      return unavailable_type_conversion{};
    }
  } else {
    return unavailable_type_conversion{};
  }
}

// The address of an lvalue source, for a pointer target it converts to.
template <typename Target, typename Source, typename Access>
constexpr auto address_path() {
  if constexpr (std::is_pointer_v<Target> &&
                std::is_lvalue_reference_v<Source> &&
                std::is_convertible_v<
                    std::add_pointer_t<std::remove_reference_t<Source>>,
                    Target>) {
    return conversion_leaf<address, Target, borrow>{};
  } else {
    return unavailable_type_conversion{};
  }
}

// The address of the element of an lvalue wrapper, for a pointer target it
// converts to.
template <typename Target, typename Source, typename Access>
constexpr auto element_ptr_path() {
  if constexpr (std::is_pointer_v<Target> &&
                std::is_lvalue_reference_v<Source> &&
                has_element_ptr_v<Source>) {
    if constexpr (std::is_convertible_v<
                      decltype(type_traits<remove_cvref_t<Source>>::get(
                          std::declval<Source>())),
                      Target>) {
      return conversion_leaf<element_ptr, Target, borrow>{};
    } else {
      return unavailable_type_conversion{};
    }
  } else {
    return unavailable_type_conversion{};
  }
}

// The element of a handle that shares it is borrowed, of a consumed handle
// too: another handle may hold it.
template <typename Source, typename Access>
using element_access_t =
    std::conditional_t<std::is_base_of_v<semantics::shared_owner,
                                         type_traits<remove_cvref_t<Source>>>,
                       borrow, Access>;

template <typename Target, typename Source, typename Access>
constexpr auto borrow_element_path() {
  if constexpr (has_borrow_element_v<Source>) {
    using inner = path_t<Target,
                         decltype(type_traits<remove_cvref_t<Source>>::borrow(
                             std::declval<Source>())),
                         element_access_t<Source, Access>>;
    return available_t<conversion_unwrap<borrow_element, inner>, inner>{};
  } else {
    return unavailable_type_conversion{};
  }
}

// The element of an owning handle Target, made of a Source: Source itself
// when it derives from the element of a handle of an object. An array holds
// its element exactly.
template <typename Target, typename Source,
          typename Element = std::remove_cv_t<
              typename type_traits<remove_cvref_t<Target>>::value_type>>
using held_t =
    std::conditional_t<!is_array_like_type_v<remove_cvref_t<Target>> &&
                           std::is_base_of_v<Element, remove_cvref_t<Source>>,
                       remove_cvref_t<Source>, Element>;

// A new owning handle of a consumed value: of an object, of a wrapper, or an
// array of it, the rest of a bounded one value-initialized. A pointer is not
// held: a handle takes it over instead. A wrapper is held by a handle of it,
// as itself or converted to the wrapper of its kind the handle holds, such as
// a handle of an interface of its element; another handle is made of what the
// wrapper holds.
template <typename Target, typename Source>
inline constexpr bool boxes_source_v = [] {
  using source_type = remove_cvref_t<Source>;
  if constexpr (!type_traits<source_type>::enabled) {
    return true;
  } else if constexpr (!type_traits<remove_cvref_t<Target>>::enabled) {
    return false;
  } else {
    using held = std::remove_cv_t<
        typename type_traits<remove_cvref_t<Target>>::value_type>;
    if constexpr (std::is_same_v<held, source_type>) {
      return true;
    } else if constexpr (type_traits<held>::enabled) {
      return std::is_same_v<
          typename type_traits<source_type>::template rebind_t<
              typename type_traits<held>::value_type>,
          held>;
    } else {
      return false;
    }
  }
}();

// Whether a handle of the kind of Target can be made of a held value: asked
// only of one that can exist, as asking it of another instantiates it.
template <typename Target, typename Held> constexpr bool box_makeable() {
  if constexpr (is_formable_v<Held>) {
    return handle_makeable<
        typename type_traits<remove_cvref_t<Target>>::template rebind_t<Held>,
        Held>::value;
  } else {
    return false;
  }
}

template <typename Target, typename Source, typename Access>
constexpr auto box_path() {
  using source_type = remove_cvref_t<Source>;
  using target_type = remove_cvref_t<Target>;
  if constexpr (std::is_same_v<Access, consume> &&
                !std::is_reference_v<Target> &&
                !std::is_lvalue_reference_v<Source> &&
                type_traits<target_type>::is_owning_handle &&
                std::is_object_v<source_type> &&
                !std::is_pointer_v<source_type> &&
                !std::is_array_v<source_type> &&
                boxes_source_v<Target, Source>) {
    using held = held_t<Target, Source>;
    if constexpr (box_makeable<Target, held>()) {
      using inner = path_t<held, Source, Access>;
      return available_t<conversion_wrap<box<held>, Target, inner>, inner>{};
    } else {
      return unavailable_type_conversion{};
    }
  } else {
    return unavailable_type_conversion{};
  }
}

// The operations, in the order their paths are tried. A consumed wrapper is
// boxed before its element is borrowed: what holds it is made of it, rather
// than of a new element.
enum class op : std::size_t {
  basic,
  direct,
  wrap_alternative,
  held_alternative,
  element_or_empty,
  wrap_optional,
  decay,
  array_view,
  address,
  element_ptr,
  box,
  borrow_element,
  none
};

template <op Op, typename Target, typename Source, typename Access>
constexpr auto path() {
  if constexpr (Op == op::basic) {
    return basic_path<Target, Source, Access>();
  } else if constexpr (Op == op::direct) {
    return direct_path<Target, Source, Access>();
  } else if constexpr (Op == op::wrap_alternative) {
    return wrap_alternative_path<Target, Source, Access>();
  } else if constexpr (Op == op::held_alternative) {
    return held_alternative_path<Target, Source, Access>();
  } else if constexpr (Op == op::element_or_empty) {
    return element_or_empty_path<Target, Source, Access>();
  } else if constexpr (Op == op::wrap_optional) {
    return wrap_optional_path<Target, Source, Access>();
  } else if constexpr (Op == op::decay) {
    return decay_path<Target, Source, Access>();
  } else if constexpr (Op == op::array_view) {
    return array_view_path<Target, Source, Access>();
  } else if constexpr (Op == op::address) {
    return address_path<Target, Source, Access>();
  } else if constexpr (Op == op::element_ptr) {
    return element_ptr_path<Target, Source, Access>();
  } else if constexpr (Op == op::box) {
    return box_path<Target, Source, Access>();
  } else {
    return borrow_element_path<Target, Source, Access>();
  }
}

// The first path, from Op on, available with the access. The paths of later
// operations are not instantiated once one is.
template <typename Target, typename Source, typename Access, op Op = op::basic>
constexpr auto select() {
  if constexpr (Op == op::none) {
    return unavailable_type_conversion{};
  } else {
    using conversion =
        conversion_for_access_t<decltype(path<Op, Target, Source, Access>()),
                                Access>;
    if constexpr (conversion::available) {
      return conversion{};
    } else {
      return select<Target, Source, Access,
                    static_cast<op>(static_cast<std::size_t>(Op) + 1)>();
    }
  }
}

template <typename Target, typename Source, typename Access, bool Default>
struct type_conversion_path {
  using type = decltype(select<Target, Source, Access>());
  static constexpr bool available = type::available;
};

// The conversion a custom type_conversion_traits declares, with the access it
// requires.
template <typename Target, typename Source, typename Access>
struct type_conversion_path<Target, Source, Access, false> {
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
      std::conditional_t<converts,
                         convert_t<Target, Source, required_access, Source>,
                         unavailable_type_conversion>,
      Access>;
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
