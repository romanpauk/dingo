//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/type/type_list.h>

#include <type_traits>

namespace dingo {
struct borrow {};
struct consume {};

namespace detail {
template <typename Target, typename Source, typename Conversion>
struct type_resolution;

template <typename Operation>
using operation_cache_types_t = typename Operation::cache_types;

template <typename Operation, typename Storage>
using operation_temporary_types_t =
    typename Operation::template temporary_types<Storage>;

template <typename Operation>
inline constexpr bool operation_requires_source_retention_v =
    Operation::requires_source_retention;

template <typename Source, typename Target,
          bool SourcePointer = std::is_pointer_v<std::remove_cv_t<Source>>,
          bool TargetPointer = std::is_pointer_v<std::remove_cv_t<Target>>>
struct is_same_qualification_shape : std::false_type {};

template <typename Source, typename Target>
struct is_same_qualification_shape<Source, Target, false, false>
    : std::is_same<std::remove_cv_t<Source>, std::remove_cv_t<Target>> {};

template <typename Source, typename Target>
struct is_same_qualification_shape<Source, Target, true, true>
    : is_same_qualification_shape<
          std::remove_pointer_t<std::remove_cv_t<Source>>,
          std::remove_pointer_t<std::remove_cv_t<Target>>> {};

template <typename Target, typename Request,
          bool SameShape = std::is_lvalue_reference_v<Target> ==
                               std::is_lvalue_reference_v<Request> &&
                           std::is_rvalue_reference_v<Target> ==
                               std::is_rvalue_reference_v<Request> &&
                           is_same_qualification_shape<
                               std::remove_reference_t<Target>,
                               std::remove_reference_t<Request>>::value,
          bool PlainValue =
              !std::is_reference_v<Target> && !std::is_pointer_v<Target>>
struct is_resolution_request : std::false_type {};

template <typename Target, typename Request>
struct is_resolution_request<Target, Request, true, true> : std::true_type {};

template <typename Target, typename Request>
struct is_resolution_request<Target, Request, true, false>
    : std::is_convertible<Target, Request> {};

template <typename Target, typename Request>
inline constexpr bool is_resolution_request_v =
    is_resolution_request<Target, Request>::value;
} // namespace detail

// A resolution is a route to a resolved result. A route to an object address
// (a pointer target) may also serve the reference request of its object and a
// value request that copies it: the binding yields the object address and the
// request site applies the category.
template <typename Target, typename Operation, bool ServesReference = false,
          bool ServesValue = false>
struct resolution {
  static_assert(!(ServesReference || ServesValue) || std::is_pointer_v<Target>,
                "only a route to an object address can serve other requests");

  using target_type = Target;
  using result_type = std::remove_reference_t<Target>;
  using operation = Operation;

  static constexpr bool serves_reference = ServesReference;
  static constexpr bool serves_value = ServesValue;
};

namespace detail {
// The reference form of the object an address route resolves to.
template <typename Resolution>
using resolution_reference_target_t = std::add_lvalue_reference_t<
    std::remove_pointer_t<typename Resolution::target_type>>;

// The value form of the object an address route resolves to.
template <typename Resolution>
using resolution_value_target_t =
    std::remove_cv_t<std::remove_pointer_t<typename Resolution::target_type>>;

template <typename Resolution, typename Request,
          bool Reference = Resolution::serves_reference &&
                           std::is_lvalue_reference_v<Request>>
struct is_resolution_request_for
    : std::bool_constant<
          is_resolution_request_v<typename Resolution::target_type, Request>> {
};

// A reference request is accepted by the reference form of the object of an
// object route.
template <typename Resolution, typename Request>
struct is_resolution_request_for<Resolution, Request, true>
    : std::bool_constant<is_resolution_request_v<
          resolution_reference_target_t<Resolution>, Request>> {};

template <typename Resolution, typename Request>
inline constexpr bool is_resolution_request_for_v =
    is_resolution_request_for<Resolution, Request>::value;
} // namespace detail

namespace detail {
template <typename Resolutions, typename Storage> struct resolution_cache_types;

template <typename Storage, typename... Resolutions>
struct resolution_cache_types<type_list<Resolutions...>, Storage> {
  using type = type_list_unique_t<type_list_cat_t<
      operation_cache_types_t<typename Resolutions::operation>...>>;
};

template <typename Resolutions, typename Storage>
using resolution_cache_types_t =
    typename resolution_cache_types<Resolutions, Storage>::type;

template <typename Resolutions, typename Storage>
struct resolution_temporary_types;

template <typename Storage, typename... Resolutions>
struct resolution_temporary_types<type_list<Resolutions...>, Storage> {
  using type = type_list_unique_t<type_list_cat_t<operation_temporary_types_t<
      typename Resolutions::operation, Storage>...>>;
};

template <typename Resolutions, typename Storage>
using resolution_temporary_types_t =
    typename resolution_temporary_types<Resolutions, Storage>::type;
} // namespace detail
} // namespace dingo
