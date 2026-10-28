//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// One answer to "which route serves this request for this binding?", shared by
// the static and runtime backends. The static source reads the route type at
// compile time; the runtime source reads its index and applies it through the
// same route list. The canonical-leaf shortcut is a property of this answer,
// not a parallel trait: the cache query below reads it too.

#include <dingo/base/selected.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/base/type_list.h>
#include <dingo/resolution/resolution.h>
#include <dingo/storage/storage.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>

#include <type_traits>

namespace dingo {
namespace detail {

// The request a static request resolves: the annotation of an annotated or
// selected request does not take part in the resolution.
template <typename Request>
using unwrapped_static_request_t = typename annotated_traits<std::conditional_t<
    is_selected_v<Request>, selected_type_t<Request>, Request>>::type;

template <typename BindingModel>
inline constexpr bool binding_uses_default_conversions_v = std::is_same_v<
    typename BindingModel::conversions_type,
    conversions<typename BindingModel::storage_tag,
                typename BindingModel::registered_storage_type, runtime_type>>;

// The request leaf is the stored leaf of a binding that publishes the default
// conversion set. The cache query and the canonical route both start here.
template <typename Request, typename BindingModel>
inline constexpr bool binding_is_default_leaf_v = [] {
  using request_leaf = std::remove_cv_t<std::remove_pointer_t<
      std::remove_reference_t<unwrapped_static_request_t<Request>>>>;
  using stored_leaf = typename BindingModel::stored_leaf_type;
  return binding_uses_default_conversions_v<BindingModel> &&
         BindingModel::conversions_type::is_stable &&
         std::is_same_v<stored_leaf, std::remove_cv_t<stored_leaf>> &&
         std::is_same_v<request_leaf, stored_leaf>;
}();

template <typename Request, typename Resolutions>
struct matching_binding_resolution;

template <typename Request>
struct matching_binding_resolution<Request, type_list<>> {
  using type = void;
};

template <typename Request, typename Head, typename... Tail>
struct matching_binding_resolution<Request, type_list<Head, Tail...>> {
private:
  using unwrapped_request = unwrapped_static_request_t<Request>;

public:
  using type = std::conditional_t<
      is_resolution_request_for_v<Head, unwrapped_request>, Head,
      typename matching_binding_resolution<Request, type_list<Tail...>>::type>;
};

// Resolution target shapes are disjoint, so static lookup only needs to form
// the category that can match the request. Reference and pointer requests are
// served by the same object routes.
template <typename Request, typename Interface, typename Storage,
          typename RequestType =
              std::remove_cv_t<unwrapped_static_request_t<Request>>>
struct request_binding_resolutions {
  using type =
      typename binding_value_resolutions<Interface,
                                         storage_shape_t<Storage>>::type;
};

template <typename Request, typename Interface, typename Storage,
          typename RequestType>
struct request_binding_resolutions<Request, Interface, Storage, RequestType &> {
  using type =
      typename binding_object_resolutions<Interface,
                                          storage_shape_t<Storage>>::type;
};

template <typename Request, typename Interface, typename Storage,
          typename RequestType>
struct request_binding_resolutions<Request, Interface, Storage,
                                   RequestType &&> {
  using type = typename binding_rvalue_reference_resolutions<
      Interface, storage_shape_t<Storage>>::type;
};

template <typename Request, typename Interface, typename Storage,
          typename RequestType>
struct request_binding_resolutions<Request, Interface, Storage, RequestType *> {
  using type =
      typename binding_object_resolutions<Interface,
                                          storage_shape_t<Storage>>::type;
};

// The route of a stable default leaf request is the route the storage publishes
// for the leaf: the object route when the leaf's reference and pointer
// requests are served together, otherwise the route of the request's category.
template <typename Interface, typename Shape, bool Shared, bool Reference>
struct direct_default_leaf_route {
  using type = type_list<typename storage_resolution<
      std::conditional_t<Reference, runtime_type &, runtime_type *>, Interface,
      Shape, borrow, false>::type>;
};

template <typename Interface, typename Shape, bool Reference>
struct direct_default_leaf_route<Interface, Shape, true, Reference> {
private:
  using conversions = typename Shape::conversions;
  static constexpr bool serves_value =
      type_list_contains_v<runtime_type, typename conversions::value_types> &&
      type_list_size_v<typename conversions::rvalue_reference_types> == 0 &&
      is_default_value_request<Interface, Shape, runtime_type *>();

public:
  using type =
      typename shared_object_resolutions<type_list<runtime_type *>, Interface,
                                         Shape, serves_value>::type;
};

template <typename Interface, typename Shape,
          typename Conversions = typename Shape::conversions>
constexpr bool is_default_leaf_object_v =
    type_list_contains_v<runtime_type &,
                         typename Conversions::lvalue_reference_types> &&
    type_list_contains_v<runtime_type *, typename Conversions::pointer_types> &&
    is_default_object_request<Interface, Shape, runtime_type *>();

template <typename Request, typename Interface, typename Storage>
struct direct_default_leaf_request_resolution
    : direct_default_leaf_route<
          Interface, storage_shape_t<Storage>,
          is_default_leaf_object_v<Interface, storage_shape_t<Storage>>,
          std::is_lvalue_reference_v<unwrapped_static_request_t<Request>>> {};

// A stable default leaf request always selects the canonical runtime_type
// storage route, so avoid constructing and searching every published route.
// The same predicate disables the conversion cache: the route hands the stored
// object out directly.
template <typename Request, typename BindingModel>
inline constexpr bool binding_is_canonical_leaf_v = [] {
  using request = unwrapped_static_request_t<Request>;
  using request_value = std::remove_reference_t<request>;
  using conversions = typename BindingModel::conversions_type;
  constexpr bool publishes_request_category = [] {
    if constexpr (std::is_lvalue_reference_v<request>) {
      return type_list_contains_v<runtime_type &,
                                  typename conversions::lvalue_reference_types>;
    } else if constexpr (std::is_pointer_v<request_value>) {
      return type_list_contains_v<runtime_type *,
                                  typename conversions::pointer_types>;
    } else {
      return false;
    }
  }();
  return binding_is_default_leaf_v<Request, BindingModel> &&
         publishes_request_category;
}();

template <typename Request, typename Interface, typename Storage,
          bool Canonical = false>
struct route_candidates
    : request_binding_resolutions<Request, Interface, Storage> {};

template <typename Request, typename Interface, typename Storage>
struct route_candidates<Request, Interface, Storage, true>
    : direct_default_leaf_request_resolution<Request, Interface, Storage> {};

// The one route answer. `type` is the selected route, or void when no route
// serves the request. The runtime source uses the same list to compute the
// dispatch index, and matches by descriptor because the request is erased.
template <typename Request, typename Interface, typename BindingModel>
struct binding_route_query {
private:
  using candidates = typename route_candidates<
      Request, Interface, typename BindingModel::storage_type,
      binding_is_canonical_leaf_v<Request, BindingModel>>::type;

public:
  using type = typename matching_binding_resolution<Request, candidates>::type;
  static constexpr bool available = !std::is_void_v<type>;
};

template <typename Request, typename InterfaceBinding>
struct binding_supports_request {
private:
  using raw_interface_type = typename annotated_traits<
      typename InterfaceBinding::interface_type>::type;

public:
  using type = typename binding_route_query<
      Request, raw_interface_type,
      typename InterfaceBinding::binding_model_type>::type;

  static constexpr bool value = !std::is_void_v<type>;
};

template <typename Request, typename InterfaceBinding>
inline constexpr bool binding_supports_request_v =
    binding_supports_request<Request, InterfaceBinding>::value;

// The runtime form of the same match: the request is erased, so the route list
// is searched by descriptor instead of by type.
template <typename Resolution>
constexpr bool matches_resolution_request(type_descriptor requested_type) {
  if (matches_qualification_conversion(
          describe_type<typename Resolution::target_type>(), requested_type)) {
    return true;
  }
  // An object route serves the reference form and the copy of its object as
  // well.
  if constexpr (Resolution::serves_reference) {
    if (matches_qualification_conversion(
            describe_type<resolution_reference_target_t<Resolution>>(),
            requested_type)) {
      return true;
    }
  }
  if constexpr (Resolution::serves_value) {
    return matches_qualification_conversion(
        describe_type<resolution_value_target_t<Resolution>>(), requested_type);
  } else {
    return false;
  }
}

} // namespace detail
} // namespace dingo
