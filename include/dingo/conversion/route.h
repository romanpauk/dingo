//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/type_list.h>

#include <type_traits>

namespace dingo {
namespace detail {
template <typename Source, typename Conversion> struct type_resolution;

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

// A resolution is a route to a resolved result.
template <typename Target, typename Operation> struct resolution {
  using target_type = Target;
  using result_type = std::remove_reference_t<Target>;
  using operation = Operation;
};

} // namespace dingo
