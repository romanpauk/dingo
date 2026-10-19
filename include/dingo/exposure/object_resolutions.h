//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>
#include <dingo/conversion/access.h>
#include <dingo/conversion/type_conversion_traits.h>
#include <dingo/exposure/resolutions.h>
#include <dingo/exposure/source.h>
#include <dingo/resolution/operation.h>
#include <dingo/resolution/resolution.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
namespace detail {
// A route to an object that exists in its source presents the same object
// whether the request takes a reference or a pointer. The category is applied
// by the request site, so one route to the object address serves both.
//
// Taking the address of an object and binding a reference to it select the
// same object when the conversion is the default pointer conversion of the
// source: the object of an lvalue source, or the pointee of a pointer source.
// Every other conversion (wrappers, alternatives, arrays, retained or custom
// conversions) selects its object by its own rule and keeps a route per
// category.
// The object a pointer target and a source share, when the source is an lvalue
// or a pointer that the target is convertible from.
template <typename Target, typename Source>
inline constexpr bool is_object_address_candidate_v =
    std::is_pointer_v<Target> &&
    !std::is_array_v<std::remove_pointer_t<Target>> &&
    !std::is_pointer_v<std::remove_cv_t<std::remove_pointer_t<Target>>> &&
    (std::is_lvalue_reference_v<Source>
         ? (!std::is_array_v<std::remove_reference_t<Source>> &&
            !std::is_pointer_v<
                std::remove_cv_t<std::remove_reference_t<Source>>> &&
            std::is_convertible_v<std::remove_reference_t<Source> *, Target> &&
            // An alternative source selects its alternative, unless the object
            // is the alternative itself.
            (!is_alternative_type_v<std::remove_reference_t<Source>> ||
             std::is_same_v<
                 std::remove_cv_t<std::remove_pointer_t<Target>>,
                 std::remove_cv_t<std::remove_reference_t<Source>>>) &&
            // A direct conversion has priority over taking the address.
            !std::is_constructible_v<Target, Source> &&
            !std::is_constructible_v<Target,
                                     const std::remove_reference_t<Source> &>)
         : (std::is_pointer_v<Source> &&
            !std::is_void_v<std::remove_pointer_t<Source>> &&
            std::is_convertible_v<Source, Target>));

template <typename Target, typename Source>
constexpr bool is_default_object_route() {
  bool result = false;
  if constexpr (is_object_address_candidate_v<Target, Source>) {
    using object = std::remove_pointer_t<Target>;
    if constexpr (std::is_lvalue_reference_v<Source>) {
      using source = std::remove_cv_t<std::remove_reference_t<Source>>;
      result = is_default_type_conversion<
                   type_conversion_traits<Target, source>>::value &&
               is_default_type_conversion<
                   type_conversion_traits<object &, source>>::value;
    } else {
      using pointee = std::remove_cv_t<std::remove_pointer_t<Source>>;
      result = is_default_type_conversion<
                   type_conversion_traits<Target, Source>>::value &&
               is_default_type_conversion<
                   type_conversion_traits<object &, pointee>>::value;
    }
  }
  return result;
}

// A pointer request pairs with the reference request of the same object when
// the route of the pointer request is the default object route.
template <typename Interface, typename Storage, typename Pointer>
constexpr bool is_default_object_request() {
  using source = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<
      typename storage_source_traits<Storage>::borrowed_type>>>;
  if constexpr (type_traits<source>::enabled &&
                !type_traits<std::remove_cv_t<Interface>>::enabled &&
                !type_traits<std::remove_pointer_t<Pointer>>::enabled) {
    // The object of a wrapper source is reached through the wrapper traits,
    // unless the published object is a wrapper itself.
    return false;
  } else {
    return is_default_object_route<
        storage_resolution_target_t<Pointer, Interface, Storage, false>,
        storage_resolution_source_t<Storage, borrow>>();
  }
}

// The value request that copies an object is served by the object route when
// the copy is the identity copy of the object: the value is the stored object
// itself, copied from an lvalue source or from the pointee of a pointer source
// by the default conversions. Every other value (derived to base copies,
// wrapper and alternative conversions, custom conversions) keeps a route of its
// own.
// The value a pointer target and a source share, when the value is copied from
// an lvalue source or from the pointee of a pointer source.
template <typename Target, typename Source>
inline constexpr bool is_value_copy_candidate_v =
    std::is_pointer_v<Target> &&
    !std::is_pointer_v<std::remove_cv_t<std::remove_pointer_t<Target>>> &&
    !std::is_array_v<std::remove_cv_t<std::remove_pointer_t<Target>>> &&
    (std::is_lvalue_reference_v<Source>
         ? (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<Target>>,
                           std::remove_cv_t<std::remove_reference_t<Source>>> &&
            !std::is_volatile_v<std::remove_reference_t<Source>>)
         : (std::is_pointer_v<Source> &&
            std::is_same_v<std::remove_cv_t<std::remove_pointer_t<Target>>,
                           std::remove_cv_t<std::remove_pointer_t<Source>>> &&
            !std::is_volatile_v<std::remove_pointer_t<Source>> &&
            // A pointer source is copied from its pointee unless the value is
            // constructed from the pointer or selects its source by a
            // structure of its own.
            !is_alternative_type_v<std::remove_pointer_t<Source>> &&
            !type_traits<
                std::remove_cv_t<std::remove_pointer_t<Source>>>::enabled &&
            !std::is_constructible_v<
                std::remove_cv_t<std::remove_pointer_t<Target>>, Source>));

template <typename Target, typename Source>
constexpr bool is_default_value_route() {
  bool result = false;
  if constexpr (is_value_copy_candidate_v<Target, Source>) {
    using value = std::remove_cv_t<std::remove_pointer_t<Target>>;
    if constexpr (std::is_lvalue_reference_v<Source>) {
      result = is_copy_constructible_v<value> &&
               is_default_type_conversion<
                   type_conversion_traits<value, value>>::value;
    } else {
      result = is_copy_constructible_v<value> &&
               is_default_type_conversion<
                   type_conversion_traits<value, value>>::value &&
               is_default_type_conversion<
                   type_conversion_traits<value, Source>>::value;
    }
  }
  return result;
}

template <typename Interface, typename Storage, typename Pointer>
constexpr bool is_default_value_request() {
  return is_default_value_route<
      storage_resolution_target_t<Pointer, Interface, Storage, false>,
      storage_resolution_source_t<Storage, borrow>>();
}

// Splits the requests of a storage that has an object into the pointer requests
// whose route also serves their reference request (and their copy), and the
// requests that keep a route of their own category. A reference, a pointer and
// a value request name the same object when they differ only in their category.
template <typename Interface, typename Storage, typename References,
          typename Pointers, typename Values, bool Consumes>
struct pair_object_requests;

template <typename Interface, typename Storage, typename... References,
          typename... Pointers, typename... Values, bool Consumes>
struct pair_object_requests<Interface, Storage, type_list<References...>,
                            type_list<Pointers...>, type_list<Values...>,
                            Consumes> {
private:
  template <typename Pointer>
  static constexpr bool has_reference_request =
      (std::is_same_v<std::remove_pointer_t<Pointer>,
                      std::remove_reference_t<References>> ||
       ...);

  template <typename Pointer>
  static constexpr bool has_value_request =
      (std::is_same_v<std::remove_pointer_t<Pointer>, Values> || ...);

  template <typename Pointer>
  static constexpr bool shared_pointer =
      has_reference_request<Pointer> &&
      is_default_object_request<Interface, Storage, Pointer>();

  // A consumed storage hands out its values, so its values are not copies of
  // an object.
  template <typename Pointer> static constexpr bool is_value_pointer() {
    if constexpr (Consumes || !shared_pointer<Pointer> ||
                  !has_value_request<Pointer>) {
      return false;
    } else {
      return is_default_value_request<Interface, Storage, Pointer>();
    }
  }

  template <typename Pointer>
  static constexpr bool value_pointer = is_value_pointer<Pointer>();

  // The pointer request of the object, if any, decides.
  template <typename Reference>
  static constexpr bool shared_reference =
      (false || ... ||
       (std::is_same_v<std::remove_reference_t<Reference>,
                       std::remove_pointer_t<Pointers>> &&
        shared_pointer<Pointers>));

  template <typename Value>
  static constexpr bool shared_value =
      (false || ... ||
       (std::is_same_v<Value, std::remove_pointer_t<Pointers>> &&
        value_pointer<Pointers>));

public:
  using shared_pointers = type_list_cat_t<
      std::conditional_t<shared_pointer<Pointers> && !value_pointer<Pointers>,
                         type_list<Pointers>, type_list<>>...>;
  using value_pointers =
      type_list_cat_t<std::conditional_t<value_pointer<Pointers>,
                                         type_list<Pointers>, type_list<>>...>;
  using references = type_list_cat_t<std::conditional_t<
      shared_reference<References>, type_list<>, type_list<References>>...>;
  using pointers =
      type_list_cat_t<std::conditional_t<shared_pointer<Pointers>, type_list<>,
                                         type_list<Pointers>>...>;
  using values =
      type_list_cat_t<std::conditional_t<shared_value<Values>, type_list<>,
                                         type_list<Values>>...>;
};

// The route that serves both the reference and the pointer request of an
// object, and the copy of the object when it is a value route as well. It is
// the default conversion of the source to the object address, which needs no
// conversion planning.
template <typename Target, typename Source>
using default_object_conversion_t =
    std::conditional_t<std::is_lvalue_reference_v<Source>,
                       address_type_conversion<Target, Source>,
                       traits_type_conversion<Target, Source>>;

template <typename Pointers, typename Interface, typename Storage,
          bool ServesValue>
struct shared_object_resolutions;

template <typename... Pointers, typename Interface, typename Storage,
          bool ServesValue>
struct shared_object_resolutions<type_list<Pointers...>, Interface, Storage,
                                 ServesValue> {
private:
  using source = storage_resolution_source_t<Storage, borrow>;

  // The target of a route is formed once and named by the route.
  template <typename Target>
  using route_of =
      resolution<Target,
                 type_resolution<Target, source,
                                 default_object_conversion_t<Target, source>>,
                 true, ServesValue>;

public:
  using type = type_list<route_of<
      storage_resolution_target_t<Pointers, Interface, Storage, false>>...>;
};

// Whether a shared route already provides a route of a category for the same
// object, such as the route of an interface that is the stored object.
template <typename Route, typename Shared, bool Reference>
struct is_provided_by_shared_route : std::false_type {};

template <typename Route, typename... Shared, bool Reference>
struct is_provided_by_shared_route<Route, type_list<Shared...>, Reference>
    : std::bool_constant<(
          (Reference ? std::is_same_v<
                           std::remove_reference_t<typename Route::target_type>,
                           std::remove_pointer_t<typename Shared::target_type>>
                     : std::is_same_v<typename Route::target_type,
                                      typename Shared::target_type>) ||
          ...)> {};

template <typename Routes, typename Shared, bool Reference>
struct without_shared_forms_impl;

template <typename... Routes, typename Shared, bool Reference>
struct without_shared_forms_impl<type_list<Routes...>, Shared, Reference> {
  using type = type_list_cat_t<std::conditional_t<
      is_provided_by_shared_route<Routes, Shared, Reference>::value,
      type_list<>, type_list<Routes>>...>;
};

template <typename Routes, typename Shared, bool Reference>
struct without_shared_forms
    : without_shared_forms_impl<Routes, Shared, Reference> {};

template <typename Routes, bool Reference>
struct without_shared_forms<Routes, type_list<>, Reference> {
  using type = Routes;
};

// The reference and pointer routes of a storage that publishes only one of the
// two categories, which have no object to share.
template <typename Interface, typename Storage, typename Conversions>
struct category_object_resolutions {
private:
  using interface_routes = interface_resolutions<Interface, Storage>;

public:
  using type = type_list_cat_t<
      type_list_merge_t<
          typename interface_routes::lvalue_reference_resolutions,
          storage_resolutions_t<typename Conversions::lvalue_reference_types,
                                Interface, Storage, borrow>>,
      type_list_merge_t<
          typename interface_routes::pointer_resolutions,
          storage_resolutions_t<typename Conversions::pointer_types, Interface,
                                Storage, borrow>>>;
};

// The pairing of the requests of a storage that publishes both references and
// pointers.
template <typename Interface, typename Storage, typename Conversions>
using object_request_pairing = pair_object_requests<
    Interface, Storage, typename Conversions::lvalue_reference_types,
    typename Conversions::pointer_types, typename Conversions::value_types,
    type_list_size_v<typename Conversions::rvalue_reference_types> != 0>;

// The reference and pointer routes of a storage that publishes both, where the
// route of an object serves both categories when the object is the same.
template <typename Interface, typename Storage, typename Conversions>
struct paired_object_resolutions {
private:
  using pairing = object_request_pairing<Interface, Storage, Conversions>;
  using interface_routes = interface_resolutions<Interface, Storage>;

  using shared_routes = type_list_cat_t<
      typename shared_object_resolutions<typename pairing::shared_pointers,
                                         Interface, Storage, false>::type,
      typename shared_object_resolutions<typename pairing::value_pointers,
                                         Interface, Storage, true>::type>;
  using reference_routes = typename without_shared_forms<
      type_list_merge_t<typename interface_routes::lvalue_reference_resolutions,
                        storage_resolutions_t<typename pairing::references,
                                              Interface, Storage, borrow>>,
      shared_routes, true>::type;
  using pointer_routes = typename without_shared_forms<
      type_list_merge_t<typename interface_routes::pointer_resolutions,
                        storage_resolutions_t<typename pairing::pointers,
                                              Interface, Storage, borrow>>,
      shared_routes, false>::type;

public:
  using type = type_list_cat_t<shared_routes, reference_routes, pointer_routes>;
};

template <
    typename Interface, typename Storage,
    typename Conversions = typename Storage::conversions,
    bool References =
        type_list_size_v<typename Conversions::lvalue_reference_types> != 0,
    bool Pointers = type_list_size_v<typename Conversions::pointer_types> != 0>
struct binding_object_resolutions
    : category_object_resolutions<Interface, Storage, Conversions> {};

// A storage without reference and pointer requests publishes no object.
template <typename Interface, typename Storage, typename Conversions>
struct binding_object_resolutions<Interface, Storage, Conversions, false,
                                  false> {
  using type = type_list<>;
};

template <typename Interface, typename Storage, typename Conversions>
struct binding_object_resolutions<Interface, Storage, Conversions, true, true>
    : paired_object_resolutions<Interface, Storage, Conversions> {};

// A value route that a shared route already provides: the route copies the
// stored object itself from the same source.
template <typename Route, typename Shared>
struct is_value_provided_by_shared_route : std::false_type {};

template <typename Route, typename... Shared>
struct is_value_provided_by_shared_route<Route, type_list<Shared...>>
    : std::bool_constant<(
          (Shared::serves_value &&
           std::is_same_v<typename Route::target_type,
                          resolution_value_target_t<Shared>> &&
           std::is_same_v<typename Route::operation::source_type,
                          typename Shared::operation::source_type>) ||
          ...)> {};

template <typename Routes, typename Shared> struct without_shared_values;

template <typename... Routes, typename Shared>
struct without_shared_values<type_list<Routes...>, Shared> {
  using type = type_list_cat_t<std::conditional_t<
      is_value_provided_by_shared_route<Routes, Shared>::value, type_list<>,
      type_list<Routes>>...>;
};

// The value routes a storage publishes. The values that copy the object of a
// shared route are served by that route.
template <
    typename Interface, typename Storage,
    typename Conversions = typename Storage::conversions,
    bool Shared =
        type_list_size_v<typename Conversions::lvalue_reference_types> != 0 &&
        type_list_size_v<typename Conversions::pointer_types> != 0 &&
        type_list_size_v<typename Conversions::value_types> != 0 &&
        type_list_size_v<typename Conversions::rvalue_reference_types> == 0>
struct binding_published_value_resolutions
    : binding_value_resolutions<Interface, Storage> {};

template <typename Interface, typename Storage, typename Conversions>
struct binding_published_value_resolutions<Interface, Storage, Conversions,
                                           true> {
private:
  using pairing = object_request_pairing<Interface, Storage, Conversions>;
  using shared_routes =
      typename shared_object_resolutions<typename pairing::value_pointers,
                                         Interface, Storage, true>::type;
  using converted_values = type_list_merge_t<
      typename interface_resolutions<Interface, Storage>::value_resolutions,
      typename wrapper_resolutions<Interface, Storage>::value_resolutions>;

public:
  using type = type_list_merge_t<
      typename without_shared_values<converted_values, shared_routes>::type,
      storage_resolutions_t<typename pairing::values, Interface, Storage,
                            borrow>>;
};
} // namespace detail
} // namespace dingo
