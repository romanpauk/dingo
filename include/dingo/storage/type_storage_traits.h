//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/resolution/recursion_guard.h>
#include <dingo/resolution/resolution.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/type/rebind_type.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_list.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
template <typename...> inline constexpr bool always_false_v = false;

namespace detail {
struct no_materialization_scope {
  no_materialization_scope() = default;

  template <typename... Args> explicit no_materialization_scope(Args &&...) {}
};
} // namespace detail

template <typename StorageTag, typename Type, typename = void>
struct storage_materialization_traits {
  static constexpr bool can_retain_source = false;

  template <typename Leaf, typename Context, typename Storage>
  static auto make_guard(Context &, const Storage &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
  }

  template <typename Storage> static bool retains_source(const Storage &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
    return false;
  }

  template <typename Context, typename Storage, typename Container>
  static auto materialize_source(Context &, Storage &, Container &) {
    static_assert(always_false_v<StorageTag, Type>,
                  "storage_materialization_traits must be specialized for "
                  "this storage tag");
  }
};

template <typename StorageTag, typename Type, typename U, typename = void>
struct resolution_traits {
  using value_types = type_list<>;
  using lvalue_reference_types = type_list<>;
  using rvalue_reference_types = type_list<>;
  using pointer_types = type_list<>;
};

namespace detail {
template <typename AccessTraits, typename ResolutionTraits>
struct combined_storage_types {
  using value_types = type_list_cat_t<typename AccessTraits::value_types,
                                      typename ResolutionTraits::value_types>;
  using lvalue_reference_types =
      type_list_cat_t<typename AccessTraits::lvalue_reference_types,
                      typename ResolutionTraits::lvalue_reference_types>;
  using rvalue_reference_types =
      type_list_cat_t<typename AccessTraits::rvalue_reference_types,
                      typename ResolutionTraits::rvalue_reference_types>;
  using pointer_types =
      type_list_cat_t<typename AccessTraits::pointer_types,
                      typename ResolutionTraits::pointer_types>;
};

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <typename Storage, typename = void> struct storage_source_type {
  using type = std::remove_reference_t<typename Storage::type>;
};

template <typename Storage>
struct storage_source_type<Storage,
                           std::void_t<typename Storage::resolved_type>> {
  using type = std::remove_reference_t<typename Storage::resolved_type>;
};

template <typename Storage>
using storage_source_type_t = typename storage_source_type<Storage>::type;

// The source helpers below read a storage_shape (defined further down).
template <typename Storage> struct storage_borrow_source {
private:
  using source_type = typename Storage::resolved_type;

public:
  using type = std::conditional_t<std::is_pointer_v<source_type>, source_type,
                                  source_type &>;
};

template <typename Storage>
using storage_borrow_source_t = typename storage_borrow_source<Storage>::type;

template <typename Storage>
using storage_consume_source_t = typename Storage::resolved_type &&;

template <typename Storage> struct storage_source_traits {
private:
  using stored_value_type = remove_cvref_t<typename Storage::type>;
  using resolved_type = typename Storage::resolved_type;
  using resolved_leaf_type =
      std::conditional_t<std::is_pointer_v<resolved_type>,
                         std::remove_pointer_t<resolved_type>, resolved_type>;

public:
  using stored_type = stored_value_type;
  using direct_leaf_type =
      std::remove_cv_t<std::remove_pointer_t<stored_value_type>>;
  using borrowed_type = storage_borrow_source_t<Storage>;
  using consumed_type = storage_consume_source_t<Storage>;
  using qualification_type = resolved_leaf_type;

  static constexpr bool is_pointer = std::is_pointer_v<stored_value_type>;
  static constexpr bool is_const = std::is_const_v<resolved_leaf_type>;

  template <typename Interface>
  using interface_type = copy_cv_t<resolved_leaf_type, Interface>;

  template <typename Interface>
  static constexpr bool publishes_interface =
      !is_pointer ||
      std::is_convertible_v<stored_value_type, interface_type<Interface> *>;
};

// The part of a storage that resolution shapes are computed from. Factory and
// the storage tag do not influence them, so keying resolution computation on
// the shape lets bindings that differ only in those share instantiations.
template <typename Type, typename Source, typename Conversions>
struct storage_shape {
  using type = Type;
  using resolved_type = Source;
  using conversions = Conversions;
};

template <typename Storage>
using storage_shape_t =
    storage_shape<typename Storage::type, storage_source_type_t<Storage>,
                  typename Storage::conversions>;

template <typename Target, typename Qualification> struct qualified_target {
  using type = Target;
};

template <typename Target, typename Qualification>
struct qualified_target<Target &, Qualification> {
  using type = copy_cv_t<Qualification, Target> &;
};

template <typename Target, typename Qualification>
struct qualified_target<Target &&, Qualification> {
  using type = copy_cv_t<Qualification, Target> &&;
};

template <typename Target, typename Qualification>
struct qualified_target<Target *, Qualification> {
  using type = copy_cv_t<Qualification, Target> *;
};

template <typename Target, typename Qualification>
using qualified_target_t =
    typename qualified_target<Target, Qualification>::type;

template <typename Request>
using request_qualification_t = std::conditional_t<
    std::is_reference_v<Request>, std::remove_reference_t<Request>,
    std::conditional_t<std::is_pointer_v<remove_cvref_t<Request>>,
                       std::remove_pointer_t<remove_cvref_t<Request>>,
                       Request>>;

template <typename Request, typename Interface>
using request_target_t = qualified_target_t<resolved_type_t<Request, Interface>,
                                            request_qualification_t<Request>>;

template <typename Request, typename Leaf> struct rebind_request_leaf {
private:
  using outer = outer_traits<Request>;
  using rebound = wrapper_rebind_leaf_t<typename outer::type, Leaf>;
  using qualified =
      std::conditional_t<std::is_reference_v<Request> ||
                             std::is_pointer_v<remove_cvref_t<Request>>,
                         rebound, std::remove_cv_t<rebound>>;

public:
  using type = typename outer::template rebind_t<qualified>;
};

template <typename Request, typename Leaf>
using rebind_request_leaf_t = typename rebind_request_leaf<Request, Leaf>::type;

// The target and source of the route a request is published as, before the
// conversion between them is planned.
template <typename Interface, typename Storage>
inline constexpr bool uses_stored_leaf_v =
    storage_source_traits<Storage>::is_pointer &&
    !storage_source_traits<Storage>::template publishes_interface<Interface>;

template <typename Request, typename Interface, typename Storage>
using storage_published_type_t = qualified_target_t<
    std::conditional_t<
        storage_source_traits<Storage>::is_pointer,
        rebind_request_leaf_t<
            Request, std::conditional_t<uses_stored_leaf_v<Interface, Storage>,
                                        typename storage_source_traits<
                                            Storage>::direct_leaf_type,
                                        std::remove_cv_t<Interface>>>,
        request_target_t<
            Request, std::conditional_t<uses_stored_leaf_v<Interface, Storage>,
                                        typename storage_source_traits<
                                            Storage>::direct_leaf_type,
                                        std::remove_cv_t<Interface>>>>,
    typename storage_source_traits<Storage>::qualification_type>;

template <typename Request, typename Interface, typename Storage,
          bool PublishValue>
using storage_resolution_target_t = std::conditional_t<
    PublishValue,
    remove_cvref_t<storage_published_type_t<Request, Interface, Storage>>,
    storage_published_type_t<Request, Interface, Storage>>;

template <typename Storage, typename Access>
using storage_resolution_source_t =
    std::conditional_t<std::is_same_v<Access, borrow>,
                       typename storage_source_traits<Storage>::borrowed_type,
                       typename storage_source_traits<Storage>::consumed_type>;

template <typename Request, typename Interface, typename Storage,
          typename Access, bool PublishValue>
struct storage_resolution {
  using target_type =
      storage_resolution_target_t<Request, Interface, Storage, PublishValue>;
  using conversion_source_type = storage_resolution_source_t<Storage, Access>;
  using type =
      conversion_resolution<target_type, conversion_source_type, Access>;
};

template <typename Requests, typename Interface, typename Storage,
          typename Access, bool PublishValue>
struct storage_resolutions;

template <typename Interface, typename Storage, typename Access,
          bool PublishValue>
struct storage_resolutions<type_list<>, Interface, Storage, Access,
                           PublishValue> {
  using type = type_list<>;
};

template <typename Interface, typename Storage, typename Access,
          bool PublishValue, typename Request>
struct storage_resolutions<type_list<Request>, Interface, Storage, Access,
                           PublishValue> {
private:
  using candidate =
      storage_resolution<Request, Interface, Storage, Access, PublishValue>;

public:
  using type = std::conditional_t<
      is_type_conversion_available_v<
          conversion_target_t<typename candidate::type::target_type>,
          typename candidate::conversion_source_type, Access>,
      type_list<typename candidate::type>, type_list<>>;
};

template <typename Interface, typename Storage, typename Access,
          bool PublishValue, typename First, typename Second,
          typename... Requests>
struct storage_resolutions<type_list<First, Second, Requests...>, Interface,
                           Storage, Access, PublishValue> {
private:
  template <typename Request>
  using candidate =
      storage_resolution<Request, Interface, Storage, Access, PublishValue>;

  template <typename Request>
  using selected = std::conditional_t<
      is_type_conversion_available_v<
          conversion_target_t<typename candidate<Request>::type::target_type>,
          typename candidate<Request>::conversion_source_type, Access>,
      type_list<typename candidate<Request>::type>, type_list<>>;

public:
  using type =
      type_list_unique_t<type_list_cat_t<selected<First>, selected<Second>,
                                         selected<Requests>...>>;
};

template <typename Requests, typename Interface, typename Storage,
          typename Access, bool PublishValue = false>
using storage_resolutions_t =
    typename storage_resolutions<Requests, Interface, Storage, Access,
                                 PublishValue>::type;

// Avoid probing conversions for empty storage categories.
template <bool Enabled, typename Target, typename ConversionTarget,
          typename Source, typename Access>
struct interface_resolution_if {
  using type = type_list<>;
};

template <typename Target, typename ConversionTarget, typename Source,
          typename Access>
struct interface_resolution_if<true, Target, ConversionTarget, Source, Access> {
  using type = std::conditional_t<
      is_type_conversion_available_v<ConversionTarget, Source, Access>,
      type_list<conversion_resolution<Target, Source, Access>>, type_list<>>;
};

template <bool Enabled, typename Source, typename Interface, typename Access>
struct wrapper_resolution_if {
  using type = type_list<>;
};

template <typename Source, typename Interface, typename Access>
struct wrapper_resolution_if<true, Source, Interface, Access>
    : wrapper_resolution_traits<Source, Interface, Access> {};

template <typename Interface, typename Storage> struct interface_resolutions {
private:
  using conversions = typename Storage::conversions;
  using source = storage_source_traits<Storage>;
  using interface_type = std::remove_cv_t<Interface>;
  using borrowed_interface_type =
      typename source::template interface_type<interface_type>;
  static constexpr bool interface_is_resolvable =
      type_traits<interface_type>::enabled &&
      !std::is_pointer_v<interface_type>;

  template <bool Enabled, typename Target, typename SourceType, typename Access>
  using if_convertible =
      typename interface_resolution_if<Enabled && interface_is_resolvable,
                                       Target, Target, SourceType,
                                       Access>::type;

  using borrowed_interface_value =
      if_convertible<type_list_size_v<typename conversions::value_types> != 0,
                     interface_type, typename source::borrowed_type, borrow>;
  using consumed_interface_value = if_convertible<
      type_list_size_v<typename conversions::rvalue_reference_types> != 0,
      interface_type, typename source::consumed_type, consume>;
  using consumed_interface_rvalue = typename interface_resolution_if<
      interface_is_resolvable &&
          type_list_size_v<typename conversions::rvalue_reference_types> != 0,
      interface_type &&, interface_type, typename source::consumed_type,
      consume>::type;
  using borrowed_interface_reference = if_convertible<
      type_list_size_v<typename conversions::lvalue_reference_types> != 0,
      borrowed_interface_type &, typename source::borrowed_type, borrow>;
  using borrowed_interface_pointer =
      if_convertible<type_list_size_v<typename conversions::pointer_types> != 0,
                     borrowed_interface_type *, typename source::borrowed_type,
                     borrow>;

public:
  using value_resolutions =
      type_list_cat_t<borrowed_interface_value, consumed_interface_value>;
  using lvalue_reference_resolutions = borrowed_interface_reference;
  using rvalue_reference_resolutions = consumed_interface_rvalue;
  using pointer_resolutions = borrowed_interface_pointer;
};

template <typename Interface, typename Storage> struct wrapper_resolutions {
private:
  using conversions = typename Storage::conversions;
  using source = storage_source_traits<Storage>;

  using borrowed_composition = typename wrapper_resolution_if<
      type_list_size_v<typename conversions::value_types> != 0,
      typename source::borrowed_type, Interface, borrow>::type;
  using consumed_composition = typename wrapper_resolution_if<
      type_list_size_v<typename conversions::rvalue_reference_types> != 0,
      typename source::consumed_type, Interface, consume>::type;

public:
  using value_resolutions =
      type_list_cat_t<borrowed_composition, consumed_composition>;
};

template <typename Interface, typename Storage>
struct binding_value_resolutions {
private:
  using conversions = typename Storage::conversions;
  using interface_values =
      typename interface_resolutions<Interface, Storage>::value_resolutions;
  using wrapper_values =
      typename wrapper_resolutions<Interface, Storage>::value_resolutions;
  using stored_values = storage_resolutions_t<typename conversions::value_types,
                                              Interface, Storage, borrow>;
  using consumed_values =
      storage_resolutions_t<typename conversions::rvalue_reference_types,
                            Interface, Storage, consume, true>;
  using converted_values = type_list_merge_t<interface_values, wrapper_values>;
  using published_values = type_list_merge_t<stored_values, consumed_values>;

public:
  using type = type_list_merge_t<converted_values, published_values>;
};

template <typename Interface, typename Storage>
struct binding_rvalue_reference_resolutions {
private:
  using conversions = typename Storage::conversions;

public:
  using type = type_list_merge_t<
      typename interface_resolutions<Interface,
                                     Storage>::rvalue_reference_resolutions,
      storage_resolutions_t<typename conversions::rvalue_reference_types,
                            Interface, Storage, consume>>;
};

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

  template <typename Pointer>
  using target =
      storage_resolution_target_t<Pointer, Interface, Storage, false>;

  template <typename Pointer>
  using route = resolution<
      target<Pointer>,
      type_resolution<target<Pointer>, source,
                      default_object_conversion_t<target<Pointer>, source>>,
      true, ServesValue>;

public:
  using type = type_list<route<Pointers>...>;
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

template <typename Interface, typename Shape> struct shape_resolutions {
  using value_resolutions =
      typename binding_published_value_resolutions<Interface, Shape>::type;
  using object_resolutions =
      typename binding_object_resolutions<Interface, Shape>::type;
  using rvalue_reference_resolutions =
      typename binding_rvalue_reference_resolutions<Interface, Shape>::type;
  // Target forms are disjoint across the value, object, and rvalue-reference
  // categories, so concatenating their already-unique lists cannot duplicate
  // a resolution.
  using type = type_list_cat_t<value_resolutions, object_resolutions,
                               rvalue_reference_resolutions>;
};

template <typename Interface, typename Storage>
using binding_resolutions =
    shape_resolutions<Interface, storage_shape_t<Storage>>;

} // namespace detail

template <typename StorageTag, typename Type, typename U, typename = void>
struct type_storage_traits;

template <typename StorageTag, typename Type, typename U>
struct type_storage_traits<
    StorageTag, Type, U,
    std::enable_if_t<storage_traits<StorageTag, Type, U>::enabled>>
    : detail::combined_storage_types<storage_traits<StorageTag, Type, U>,
                                     resolution_traits<StorageTag, Type, U>> {
public:
  static constexpr bool is_stable =
      storage_traits<StorageTag, Type, U>::is_stable;
};
} // namespace dingo
