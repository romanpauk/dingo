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
// How a conversion reaches its source: it borrows it, or it consumes it.
struct borrow {};
struct consume {};

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

// A route converts a source into a target in steps. A step ends the route at
// the object it has, reaches inward to an object that object holds, or makes a
// new object outward around what the rest of the route makes; the steps below
// are tried in order, and the first that applies is taken.
//
// A conversion is a route: whether it is available, what it requires of its
// source, borrow or consume, the conversions it retains in its binding, and
// how it applies to a source.
struct unavailable_type_conversion {
  static constexpr bool available = false;
  using required_access = unavailable_access;
  using cache_types = type_list<>;
};

// A route that ends at the source: Step makes the target of it.
template <typename Step, typename Target, typename RequiredAccess>
struct ending {
  static constexpr bool available = true;
  using required_access = RequiredAccess;
  using cache_types = type_list<>;

  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &, Context &, Actual &&source,
                              type_descriptor, type_descriptor) {
    return Step::template end<Target>(std::forward<Actual>(source));
  }
};

// A route that continues from what the rest of it, Next, reaches.
template <typename Next> struct continued {
  static constexpr bool available = Next::available;
  using required_access = typename Next::required_access;
  using cache_types = typename Next::cache_types;
};

// A route that continues from the object Step reaches in the source.
template <typename Step, typename Next> struct inward : continued<Next> {
  template <typename Resolver, typename Context, typename Actual>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Actual &&source, type_descriptor requested,
                              type_descriptor registered) {
    return Next::apply(resolver, context,
                       Step::reach(std::forward<Actual>(source)), requested,
                       registered);
  }
};

// A route whose Step makes the target around what the rest of it, Next, makes
// of the source, which it is given to make when it needs it, so that what Next
// returns by value constructs what Step makes in place. A step that keeps what
// it makes in the binding names it in its retained types.
template <typename Step, typename Target, typename Next>
struct outward : continued<Next> {
  using cache_types = type_list_unique_t<
      type_list_cat_t<typename Step::template retained_types<Target>,
                      typename Next::cache_types>>;

  template <typename Resolver, typename Context, typename Actual>
  static Target apply(Resolver &resolver, Context &context, Actual &&source,
                      type_descriptor requested, type_descriptor registered) {
    return Step::template make<Target>(
        resolver, context, [&]() -> decltype(auto) {
          return Next::apply(resolver, context, std::forward<Actual>(source),
                             requested, registered);
        });
  }
};

// Marks a route that constructs a new object; it applies as its route does.
template <typename Conversion> struct converted_construction : Conversion {};

// A route that goes on with Step when the rest of it, Next, is available.
template <typename Step, typename Next>
using continued_t =
    std::conditional_t<Next::available, Step, unavailable_type_conversion>;

// The route from Source to Target that Access allows. A pair with a custom
// type_conversion_traits takes only its conversion; any other pair takes the
// first step below that applies.
template <typename Target, typename Source, typename Access = consume,
          bool Default = is_default_type_conversion<
              type_conversion_traits<Target, remove_cvref_t<Source>>>::value>
struct type_conversion_path;

template <typename Target, typename Source, typename Access>
using route_t = typename type_conversion_path<Target, Source, Access>::type;

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

// A target made by a step rather than converted: an alternative type or an
// optional value.
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

// --- Steps that end the route at the source.

// The source itself.
struct identity_step {
  template <typename Target, typename Actual>
  static decltype(auto) end(Actual &&source) {
    return std::forward<Actual>(source);
  }
};

// A reference to the source, or to a base of it.
struct reference_step {
  template <typename Target, typename Actual>
  static Target &end(Actual &&source) {
    return static_cast<Target &>(source);
  }
};

// What type_conversion_traits converts the source to, given as Argument.
template <typename Source, typename Argument> struct converted_step {
  template <typename Target, typename Actual>
  static Target end(Actual &&source) {
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
using converted_t =
    ending<converted_step<Source, Argument>, Target, RequiredAccess>;

enum class basic_conversion_kind { none, identity, reference, converted };

// The default conversions that keep the source object: a reference to it or to
// its base, a copy of it or of its base, or the object moved from an rvalue.
// Every other conversion, taking the address included, is found by the steps
// after it.
//
// The predicate is a function template rather than a lambda in a member
// initializer: MSVC's pre-C++20 lambda processor fails on those lambdas for
// some instantiations.
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

// The source kept: itself, a reference to it or to its base, or a copy or a
// move of it, tried before the general search.
struct basic_route {
  template <typename Target, typename Source, typename Access,
            basic_conversion_kind Kind =
                basic_conversion_selected<Target, Source, Access>()>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, basic_conversion_kind::identity> {
    using type = ending<identity_step, Target,
                        std::conditional_t<std::is_rvalue_reference_v<Source>,
                                           consume, borrow>>;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, basic_conversion_kind::reference> {
    using type = ending<reference_step, Target, borrow>;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, basic_conversion_kind::converted> {
    using type = converted_t<Target, Source, borrow, Source>;
  };
};

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

// What the default type_conversion_traits converts the source to. Wrappers and
// alternatives are made by the steps after it instead.
struct direct_route {
  template <typename Target, typename Source, typename Access,
            bool Structural = is_structural_conversion_target_v<Target>>
  struct route {
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
        converted_t<Target, Source,
                    std::conditional_t<borrows, borrow, consume>, argument>,
        unavailable_type_conversion>;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, true> {
    using type = unavailable_type_conversion;
  };
};

// The address of an lvalue source, for a pointer target it converts to.
struct address_step {
  template <typename Target, typename Actual>
  static Target end(Actual &&source) {
    return static_cast<Target>(std::addressof(source));
  }

  template <typename Target, typename Source, typename Access> struct route {
    using type = std::conditional_t<
        std::is_pointer_v<Target> && std::is_lvalue_reference_v<Source> &&
            std::is_convertible_v<
                std::add_pointer_t<std::remove_reference_t<Source>>, Target>,
        ending<address_step, Target, borrow>, unavailable_type_conversion>;
  };
};

// The array a pointer to its first element points to, as a reference or as a
// pointer.
struct array_step {
  template <typename Target, typename Actual>
  static Target end(Actual &&source) {
    if constexpr (std::is_pointer_v<Target>) {
      return reinterpret_cast<Target>(source);
    } else {
      return *reinterpret_cast<
          std::add_pointer_t<std::remove_reference_t<Target>>>(source);
    }
  }

  template <typename Target, typename Source, typename Access> struct route {
  private:
    using target_array = std::conditional_t<std::is_pointer_v<Target>,
                                            std::remove_pointer_t<Target>,
                                            std::remove_reference_t<Target>>;
    using projected_element = std::remove_extent_t<target_array>;
    using source_type = std::remove_reference_t<Source>;
    using source_element = std::remove_pointer_t<source_type>;
    static constexpr bool projects =
        (std::is_lvalue_reference_v<Target> || std::is_pointer_v<Target>) &&
        std::is_array_v<target_array> && std::is_pointer_v<source_type> &&
        std::is_same_v<std::remove_cv_t<source_element>,
                       std::remove_cv_t<projected_element>> &&
        std::is_convertible_v<std::add_pointer_t<source_element>,
                              std::add_pointer_t<projected_element>>;

  public:
    using type =
        std::conditional_t<projects, ending<array_step, Target, borrow>,
                           unavailable_type_conversion>;
  };
};

// The address of the element of a wrapper, for a pointer target.
struct element_address_step {
  template <typename Target, typename Actual>
  static Target end(Actual &&source) {
    return static_cast<Target>(
        type_traits<remove_cvref_t<Actual>>::get(source));
  }

  template <typename Target, typename Source, typename Access, typename = void>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access,
               std::void_t<decltype(type_traits<remove_cvref_t<Source>>::get(
                   std::declval<Source>()))>> {
  private:
    using element = decltype(type_traits<remove_cvref_t<Source>>::get(
        std::declval<Source>()));

  public:
    using type =
        std::conditional_t<std::is_pointer_v<Target> &&
                               std::is_lvalue_reference_v<Source> &&
                               std::is_convertible_v<element, Target>,
                           ending<element_address_step, Target, borrow>,
                           unavailable_type_conversion>;
  };
};

// --- Steps that reach inward, to an object the source holds.

// The pointee of a pointer.
struct pointee_step {
  template <typename Actual> static decltype(auto) reach(Actual &&source) {
    return *std::forward<Actual>(source);
  }

  template <typename Target, typename Source, typename Access, typename = void>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<
      Target, Source, Access,
      std::enable_if_t<std::is_pointer_v<std::remove_reference_t<Source>> &&
                       !std::is_void_v<std::remove_pointer_t<
                           std::remove_reference_t<Source>>> &&
                       !std::is_pointer_v<Target>>> {
    using next = route_t<Target, decltype(*std::declval<Source>()), Access>;
    using type = continued_t<inward<pointee_step, next>, next>;
  };
};

// The element of a wrapper, borrowed.
struct borrowed_step {
  template <typename Actual> static decltype(auto) reach(Actual &&source) {
    return type_traits<remove_cvref_t<Actual>>::borrow(
        std::forward<Actual>(source));
  }

  template <typename Target, typename Source, typename Access, typename = void>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<
      Target, Source, Access,
      std::enable_if_t<
          is_value_borrowable_v<remove_cvref_t<Source>>,
          std::void_t<decltype(type_traits<remove_cvref_t<Source>>::borrow(
              std::declval<Source>()))>>> {
    using next = route_t<Target,
                         decltype(type_traits<remove_cvref_t<Source>>::borrow(
                             std::declval<Source>())),
                         Access>;
    using type = continued_t<inward<borrowed_step, next>, next>;
  };
};

template <typename Alternative, typename Conversion>
struct alternative_type_conversion_branch {
  using alternative_type = Alternative;
  using conversion = Conversion;
};

template <typename Target, typename Source, typename Branches>
struct alternative_type_conversion;

// The alternative a source alternative type holds, reached as it is held, so
// each alternative is a branch of its own, tried in order.
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

// The alternative a source alternative type holds: any of them for a
// reference or pointer target, otherwise the target itself when it is an
// alternative exactly once.
struct held_alternative_step {
  template <typename Target, typename Source, typename Access,
            typename Alternatives>
  struct branches;

  template <typename Target, typename Source, typename Access,
            typename... Alternatives>
  struct branches<Target, Source, Access, type_list<Alternatives...>> {
    using type = alternative_type_conversion<
        Target, Source,
        type_list<alternative_type_conversion_branch<
            Alternatives,
            route_t<Target, alternative_source_t<Alternatives, Source>,
                    Access>>...>>;
  };

  template <typename Target, typename Source, typename Access,
            bool IsAlternative =
                is_alternative_type_v<remove_cvref_t<Source>> &&
                !std::is_volatile_v<std::remove_reference_t<Source>>>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, true> {
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
    using type = typename branches<Target, Source, Access, alternatives>::type;
  };
};

// --- Steps that make a new object outward, around what the rest of the
// route makes.

// What an outward step keeps in the binding: nothing, unless it says so.
struct outward_step {
  template <typename Target> using retained_types = type_list<>;
};

// An alternative type holding the converted source as the alternative
// overload resolution selects.
template <typename Selected> struct alternative_step : outward_step {
  template <typename Target, typename Resolver, typename Context, typename Next>
  static Target make(Resolver &, Context &, Next &&next) {
    return type_traits<remove_cvref_t<Target>>::template wrap<Selected>(next());
  }
};

struct alternative_route {
  template <typename Target, typename Source, typename Access,
            bool Enabled = is_structural_conversion_target_v<Target> &&
                           is_alternative_type_v<Target>,
            typename = void>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, true,
               std::void_t<typename type_traits<
                   std::remove_cv_t<Target>>::template selected_type<Source>>> {
  private:
    using selected = typename type_traits<
        std::remove_cv_t<Target>>::template selected_type<Source>;
    using next = route_t<selected, Source, Access>;

  public:
    using type =
        continued_t<converted_construction<
                        outward<alternative_step<selected>, Target, next>>,
                    next>;
  };
};

// An optional value made of the converted source.
struct optional_step : outward_step {
  template <typename Target, typename Resolver, typename Context, typename Next>
  static Target make(Resolver &, Context &, Next &&next) {
    return type_traits<remove_cvref_t<Target>>::wrap(next());
  }
};

// An optional value made of the converted element of a source optional value,
// empty when the source is.
template <typename Target, typename Next>
struct optional_element_conversion : continued<Next> {
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
          Next::apply(resolver, context, *value, requested, registered));
    } else {
      return target::wrap(Next::apply(resolver, context, std::move(*value),
                                      requested, registered));
    }
  }
};

// The optional value a route makes, marked as constructed when the rest of the
// route constructs.
template <typename Target, typename Next, bool OfElement>
struct optional_conversion {
  using type =
      std::conditional_t<OfElement, optional_element_conversion<Target, Next>,
                         outward<optional_step, Target, Next>>;
};

template <typename Target, typename Next, bool OfElement>
struct optional_conversion<Target, converted_construction<Next>, OfElement> {
  using type = converted_construction<
      typename optional_conversion<Target, Next, OfElement>::type>;
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

struct optional_route {
  template <typename Target, typename Source, typename Access,
            bool Enabled = is_structural_conversion_target_v<Target> &&
                           is_value_wrapper_type_v<Target>>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, true> {
  private:
    static constexpr bool of_element = is_value_wrapper_type_v<Source>;
    using next =
        route_t<typename type_traits<std::remove_cv_t<Target>>::value_type,
                typename wrapper_element_source<Source>::type, Access>;

  public:
    using type = std::conditional_t<
        (of_element || !borrows_value_wrapper_v<Source>) && next::available,
        typename optional_conversion<Target, next, of_element>::type,
        unavailable_type_conversion>;
  };
};

// A wrapper the binding keeps, converted from the source, that a reference or
// a pointer target refers to.
struct retained_step {
  template <typename Target>
  using retained_types = type_list<std::remove_cv_t<std::conditional_t<
      std::is_pointer_v<Target>, std::remove_pointer_t<Target>,
      std::remove_reference_t<Target>>>>;

  template <typename Target, typename Resolver, typename Context, typename Next>
  static Target make(Resolver &resolver, Context &context, Next &&next) {
    using retained_type = type_list_head_t<retained_types<Target>>;
    auto &retained =
        resolver.template retain_conversion<retained_type>(context, next());
    if constexpr (std::is_pointer_v<Target>) {
      return std::addressof(retained);
    } else {
      return retained;
    }
  }

  template <typename Target, typename Source, typename Access, typename = void>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<
      Target, Source, Access,
      std::enable_if_t<
          (std::is_lvalue_reference_v<Target> || std::is_pointer_v<Target>) &&
          type_traits<type_list_head_t<retained_types<Target>>>::enabled &&
          !std::is_array_v<type_list_head_t<retained_types<Target>>>>> {
    using next =
        route_t<type_list_head_t<retained_types<Target>>, Source, Access>;
    using type = continued_t<outward<retained_step, Target, next>, next>;
  };
};

// A new owning handle of a consumed value, converted to the element it holds.
// A value of a class derived from the element is held as itself. It is not a
// converted_construction: a factory makes a handle in place instead of moving
// into it what it made.
template <typename Held> struct handle_step : outward_step {
  template <typename Target, typename Resolver, typename Context, typename Next>
  static Target make(Resolver &, Context &, Next &&next) {
    using handle =
        typename type_traits<remove_cvref_t<Target>>::template rebind_t<Held>;
    // The value is taken before it is moved into the handle: GCC then folds a
    // handle that does not escape, as it does the value.
    Held held(next());
    return Target(type_traits<handle>::make(std::move(held)));
  }
};

struct handle_route {
  template <typename Target, typename Source, typename Access,
            bool Enabled =
                std::is_same_v<Access, consume> &&
                !std::is_reference_v<Target> &&
                !std::is_lvalue_reference_v<Source> &&
                type_traits<remove_cvref_t<Target>>::is_owning_handle &&
                is_value_borrowable_v<remove_cvref_t<Target>> &&
                std::is_class_v<remove_cvref_t<Source>> &&
                !type_traits<remove_cvref_t<Source>>::enabled>
  struct route {
    using type = unavailable_type_conversion;
  };

  template <typename Target, typename Source, typename Access>
  struct route<Target, Source, Access, true> {
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
      using next = route_t<held, Source, Access>;
      using type = continued_t<outward<handle_step<held>, Target, next>, next>;
    };

  public:
    using type = typename select<is_formable_v<held> &&
                                 !type_traits<held>::enabled>::type;
  };
};

// The first step, in order, that applies with the access. A later step is
// named only once every earlier one has failed, so its route is not
// instantiated before.
template <typename Target, typename Source, typename Access, typename... Steps>
struct first_route {
  using type = unavailable_type_conversion;
};

template <typename Target, typename Source, typename Access, typename Step,
          typename... Steps>
struct first_route<Target, Source, Access, Step, Steps...> {
private:
  using candidate = conversion_for_access_t<
      typename Step::template route<Target, Source, Access>::type, Access>;

public:
  using type = typename std::conditional_t<
      candidate::available, std::enable_if<true, candidate>,
      first_route<Target, Source, Access, Steps...>>::type;
};

template <typename Target, typename Source, typename Access, bool Default>
struct type_conversion_path {
  using type =
      typename first_route<Target, Source, Access, basic_route, direct_route,
                           alternative_route, held_alternative_step,
                           optional_route, array_step, address_step,
                           element_address_step, pointee_step, borrowed_step,
                           retained_step, handle_route>::type;
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
                         converted_t<Target, Source, required_access, Source>,
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
