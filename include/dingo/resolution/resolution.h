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

template <typename Source, typename Target> struct same_qualification_shape {
private:
  using source = std::remove_cv_t<Source>;
  using target = std::remove_cv_t<Target>;
  static constexpr bool source_is_pointer = std::is_pointer_v<source>;
  static constexpr bool target_is_pointer = std::is_pointer_v<target>;

public:
  static constexpr bool value = [] {
    if constexpr (source_is_pointer && target_is_pointer) {
      return same_qualification_shape<std::remove_pointer_t<source>,
                                      std::remove_pointer_t<target>>::value;
    } else if constexpr (source_is_pointer || target_is_pointer) {
      return false;
    } else {
      return std::is_same_v<source, target>;
    }
  }();
};

template <typename Target, typename Request>
inline constexpr bool is_resolution_request_v = [] {
  constexpr bool same_reference_kind =
      std::is_lvalue_reference_v<Target> ==
          std::is_lvalue_reference_v<Request> &&
      std::is_rvalue_reference_v<Target> == std::is_rvalue_reference_v<Request>;
  using target_type = std::remove_reference_t<Target>;
  using request_type = std::remove_reference_t<Request>;

  if constexpr (!same_reference_kind ||
                !same_qualification_shape<target_type, request_type>::value) {
    return false;
  } else if constexpr (!std::is_reference_v<Target> &&
                       !std::is_pointer_v<Target>) {
    return true;
  } else {
    return std::is_convertible_v<Target, Request>;
  }
}();
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
