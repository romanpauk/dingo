//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/type_list.h>
#include <dingo/context/recursion_guard.h>
#include <dingo/conversion/available.h>
#include <dingo/conversion/paths.h>
#include <dingo/conversion/route.h>
#include <dingo/conversion/source.h>
#include <dingo/request/decomposition.h>
#include <dingo/type/shape.h>
#include <dingo/type/type_traits.h>

#include <type_traits>

namespace dingo {
namespace detail {
struct no_materialization_scope {};
} // namespace detail

namespace detail {
// What a storage resolves, borrowed and consumed: a pointer is borrowed as
// itself.
template <typename Storage> struct storage_source_traits {
private:
  using stored_value_type = remove_cvref_t<typename Storage::type>;
  using resolved_type = typename Storage::resolved_type;
  using resolved_leaf_type =
      std::conditional_t<std::is_pointer_v<resolved_type>,
                         std::remove_pointer_t<resolved_type>, resolved_type>;

public:
  using direct_leaf_type =
      std::remove_cv_t<std::remove_pointer_t<stored_value_type>>;
  using borrowed_type = std::conditional_t<std::is_pointer_v<resolved_type>,
                                           resolved_type, resolved_type &>;
  using consumed_type = resolved_type &&;
  using qualification_type = resolved_leaf_type;

  static constexpr bool is_pointer = std::is_pointer_v<stored_value_type>;

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
    storage_shape<typename Storage::type,
                  std::remove_reference_t<typename Storage::resolved_type>,
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

// The leaf a request is published for: the stored leaf when the interface is
// not served by a pointer source, otherwise the interface itself.
template <typename Interface, typename Storage>
using storage_published_leaf_t = std::conditional_t<
    uses_stored_leaf_v<Interface, Storage>,
    typename storage_source_traits<Storage>::direct_leaf_type,
    std::remove_cv_t<Interface>>;

// A pointer source rebinds the leaf of the request, any other source resolves
// the request type of the leaf. Only the branch of the source is formed.
template <typename Request, typename Interface, typename Storage,
          bool IsPointer = storage_source_traits<Storage>::is_pointer>
struct storage_published_type {
  using type = qualified_target_t<
      request_target_t<Request, storage_published_leaf_t<Interface, Storage>>,
      typename storage_source_traits<Storage>::qualification_type>;
};

template <typename Request, typename Interface, typename Storage>
struct storage_published_type<Request, Interface, Storage, true> {
  using type = qualified_target_t<
      rebind_request_leaf_t<Request,
                            storage_published_leaf_t<Interface, Storage>>,
      typename storage_source_traits<Storage>::qualification_type>;
};

template <typename Request, typename Interface, typename Storage>
using storage_published_type_t =
    typename storage_published_type<Request, Interface, Storage>::type;

template <typename Storage, typename Access>
using storage_resolution_source_t =
    std::conditional_t<std::is_same_v<Access, borrow>,
                       typename storage_source_traits<Storage>::borrowed_type,
                       typename storage_source_traits<Storage>::consumed_type>;

// Whether a request of Target selects the binding of Interface: a request is
// looked up by its normalized type, or by its exact type.
template <typename Target, typename Interface>
inline constexpr bool selects_interface_v =
    std::is_same_v<typename request_type<Target>::value_type, Interface> ||
    std::is_same_v<typename request_type<Target>::exact_type, Interface>;

// The resolution of Target from Source when Enabled, and none otherwise: a
// conversion is not probed for a storage category that is empty, nor for a
// request a binding of another interface serves.
template <bool Enabled, typename Target, typename Source, typename Access>
struct interface_resolution_if {
  using type = type_list<>;
};

template <typename Target, typename Source, typename Access>
struct interface_resolution_if<true, Target, Source, Access>
    : available_resolution<Target, Source, Access> {};

// The resolution of a request the storage publishes, as a value when
// PublishValue, when a request of its target selects the binding.
template <
    typename Request, typename Interface, typename Storage, typename Access,
    bool PublishValue,
    typename Target = std::conditional_t<
        PublishValue,
        remove_cvref_t<storage_published_type_t<Request, Interface, Storage>>,
        storage_published_type_t<Request, Interface, Storage>>>
using storage_resolution_t = typename interface_resolution_if<
    selects_interface_v<Target, std::remove_cv_t<Interface>>, Target,
    storage_resolution_source_t<Storage, Access>, Access>::type;

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
  using type =
      storage_resolution_t<Request, Interface, Storage, Access, PublishValue>;
};

template <typename Interface, typename Storage, typename Access,
          bool PublishValue, typename First, typename Second,
          typename... Requests>
struct storage_resolutions<type_list<First, Second, Requests...>, Interface,
                           Storage, Access, PublishValue> {
  using type = type_list_unique_t<type_list_cat_t<
      storage_resolution_t<First, Interface, Storage, Access, PublishValue>,
      storage_resolution_t<Second, Interface, Storage, Access, PublishValue>,
      storage_resolution_t<Requests, Interface, Storage, Access,
                           PublishValue>...>>;
};

template <typename Requests, typename Interface, typename Storage,
          typename Access, bool PublishValue = false>
using storage_resolutions_t =
    typename storage_resolutions<Requests, Interface, Storage, Access,
                                 PublishValue>::type;

template <bool Enabled, typename Source, typename Interface, typename Access>
struct wrapper_resolution_if {
  using type = type_list<>;
};

template <typename Source, typename Interface, typename Access>
struct wrapper_resolution_if<true, Source, Interface, Access>
    : wrapper_resolution_traits<Source, Interface, Access> {};

// The categories of requests a storage publishes resolutions for.
enum class request_category {
  value,
  lvalue_reference,
  rvalue_reference,
  pointer
};

// The interface a storage resolves directly, as opposed to the requests it
// publishes.
template <typename Interface, typename Storage> struct interface_target {
  using type = std::remove_cv_t<Interface>;
  using borrowed_type =
      typename storage_source_traits<Storage>::template interface_type<type>;
  static constexpr bool resolvable =
      type_traits<type>::enabled && !std::is_pointer_v<type>;
};

// The requests, access, interface target and source of a category of
// reference or pointer requests.
template <request_category Category, typename Interface, typename Storage>
struct object_category;

template <typename Interface, typename Storage>
struct object_category<request_category::lvalue_reference, Interface, Storage> {
  using requests = typename Storage::conversions::lvalue_reference_types;
  using access = borrow;
  using target = typename interface_target<Interface, Storage>::borrowed_type &;
  using source = typename storage_source_traits<Storage>::borrowed_type;
};

template <typename Interface, typename Storage>
struct object_category<request_category::pointer, Interface, Storage> {
  using requests = typename Storage::conversions::pointer_types;
  using access = borrow;
  using target = typename interface_target<Interface, Storage>::borrowed_type *;
  using source = typename storage_source_traits<Storage>::borrowed_type;
};

template <typename Interface, typename Storage>
struct object_category<request_category::rvalue_reference, Interface, Storage> {
  using requests = typename Storage::conversions::rvalue_reference_types;
  using access = consume;
  using target = typename interface_target<Interface, Storage>::type &&;
  using source = typename storage_source_traits<Storage>::consumed_type;
};

// The routes of a category: the interface route, when the storage publishes
// any request of the category, and the routes of the requests it publishes.
template <request_category Category, typename Interface, typename Storage>
struct category_resolutions {
private:
  using category = object_category<Category, Interface, Storage>;
  using requests = typename category::requests;

public:
  using type = type_list_merge_t<
      typename interface_resolution_if<
          interface_target<Interface, Storage>::resolvable &&
              type_list_size_v<requests> != 0,
          typename category::target, typename category::source,
          typename category::access>::type,
      storage_resolutions_t<requests, Interface, Storage,
                            typename category::access>>;
};

// The leaf a storage stores, whose value a request copies of the reference
// to it or moves out of the rvalue reference to it itself. None of a
// volatile one, which is not copied of a const reference, nor of a pointer,
// whose value is published: the pointer may be null, where no reference to
// what it points to exists.
template <typename Storage,
          typename Stored = std::remove_reference_t<typename Storage::type>>
using copied_leaf_t = std::conditional_t<
    std::is_volatile_v<Stored> || storage_source_traits<Storage>::is_pointer,
    void, std::remove_cv_t<leaf_type_t<std::remove_cv_t<Stored>>>>;

// The value resolutions of a storage but those of the leaf it stores.
template <typename Resolutions, typename Leaf> struct without_leaf_values;

template <typename... Resolutions, typename Leaf>
struct without_leaf_values<type_list<Resolutions...>, Leaf> {
  using type = type_list_cat_t<std::conditional_t<
      std::is_same_v<std::remove_cv_t<typename Resolutions::target_type>, Leaf>,
      type_list<>, type_list<Resolutions>>...>;
};

// A value is converted from the borrowed and the consumed source alike, into
// the interface or into a wrapper of it. A value of the leaf the storage
// stores is not: a request copies or moves it itself.
template <typename Interface, typename Storage>
struct category_resolutions<request_category::value, Interface, Storage> {
private:
  using conversions = typename Storage::conversions;
  using source = storage_source_traits<Storage>;
  using target = interface_target<Interface, Storage>;
  static constexpr bool borrows =
      type_list_size_v<typename conversions::value_types> != 0;
  static constexpr bool consumes =
      type_list_size_v<typename conversions::rvalue_reference_types> != 0;

  using interface_values =
      type_list_cat_t<typename interface_resolution_if<
                          target::resolvable && borrows, typename target::type,
                          typename source::borrowed_type, borrow>::type,
                      typename interface_resolution_if<
                          target::resolvable && consumes, typename target::type,
                          typename source::consumed_type, consume>::type>;
  using wrapper_values = type_list_cat_t<
      typename wrapper_resolution_if<borrows, typename source::borrowed_type,
                                     Interface, borrow>::type,
      typename wrapper_resolution_if<consumes, typename source::consumed_type,
                                     Interface, consume>::type>;
  using stored_values = storage_resolutions_t<typename conversions::value_types,
                                              Interface, Storage, borrow>;
  using consumed_values =
      storage_resolutions_t<typename conversions::rvalue_reference_types,
                            Interface, Storage, consume, true>;

public:
  using type = typename without_leaf_values<
      type_list_merge_t<type_list_merge_t<interface_values, wrapper_values>,
                        type_list_merge_t<stored_values, consumed_values>>,
      copied_leaf_t<Storage>>::type;
};

// Whether the requests a storage publishes consume what it makes for each.
template <typename Conversions, typename = void>
struct consumes_made : std::false_type {};

template <typename Conversions>
struct consumes_made<Conversions, std::void_t<decltype(Conversions::consumes)>>
    : std::bool_constant<Conversions::consumes> {};

// The lvalue reference resolutions of a storage but those of what it
// publishes an rvalue reference to: a request refers to what the storage
// makes for it through that.
template <typename LvalueResolutions, typename RvalueResolutions>
struct without_referred_resolutions;

template <typename... Lvalues, typename... Rvalues>
struct without_referred_resolutions<type_list<Lvalues...>,
                                    type_list<Rvalues...>> {
private:
  using referred = type_list<remove_cvref_t<typename Rvalues::target_type>...>;

public:
  using type = type_list_cat_t<std::conditional_t<
      type_list_contains_v<remove_cvref_t<typename Lvalues::target_type>,
                           referred>,
      type_list<>, type_list<Lvalues>>...>;
};

// The resolutions a storage publishes of a category. A storage that makes a
// value for each request publishes no lvalue reference to what it publishes
// an rvalue reference to.
template <request_category Category, typename Interface, typename Storage>
struct published_resolutions
    : category_resolutions<Category, Interface, Storage> {};

template <typename Interface, typename Storage>
struct published_resolutions<request_category::lvalue_reference, Interface,
                             Storage> {
private:
  using all = typename category_resolutions<request_category::lvalue_reference,
                                            Interface, Storage>::type;

public:
  using type = typename std::conditional_t<
      consumes_made<typename Storage::conversions>::value,
      without_referred_resolutions<
          all, typename category_resolutions<request_category::rvalue_reference,
                                             Interface, Storage>::type>,
      std::enable_if<true, all>>::type;
};

template <request_category Category, typename Interface, typename Storage>
using category_resolutions_t =
    typename published_resolutions<Category, Interface, Storage>::type;

template <typename Interface, typename Shape> struct shape_resolutions {
  using value_resolutions =
      category_resolutions_t<request_category::value, Interface, Shape>;
  using rvalue_reference_resolutions =
      category_resolutions_t<request_category::rvalue_reference, Interface,
                             Shape>;
  using object_resolutions = type_list_cat_t<
      category_resolutions_t<request_category::lvalue_reference, Interface,
                             Shape>,
      category_resolutions_t<request_category::pointer, Interface, Shape>>;
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

} // namespace dingo
