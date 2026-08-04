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

template <typename Operation, typename Storage>
using operation_cache_types_t =
    typename Operation::template cache_types<Storage>;

template <typename Operation, typename Storage>
using operation_temporary_types_t =
    typename Operation::template temporary_types<Storage>;

template <typename Operation, typename Storage>
inline constexpr bool operation_requires_source_retention_v =
    Operation::template requires_source_retention<Storage>;

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

template <typename Target, typename Operation> struct resolution {
  using target_type = Target;
  using result_type = std::remove_reference_t<Target>;
  using operation = Operation;
};

namespace detail {
template <typename Resolutions, typename Storage> struct resolution_cache_types;

template <typename Storage, typename... Resolutions>
struct resolution_cache_types<type_list<Resolutions...>, Storage> {
  using type = type_list_unique_t<type_list_cat_t<
      operation_cache_types_t<typename Resolutions::operation, Storage>...>>;
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
