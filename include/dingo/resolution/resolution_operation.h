//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>

#include <dingo/base/exceptions.h>
#include <dingo/base/type_list.h>
#include <dingo/resolution/materialized_source.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_traits.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
template <typename MaterializedSource>
using materialized_source_traits_t = materialized_source_traits<
    std::remove_cv_t<std::remove_reference_t<MaterializedSource>>>;

template <typename MaterializedSource>
decltype(auto) materialized_value(MaterializedSource &&source) {
  return materialized_source_traits_t<MaterializedSource>::value(
      std::forward<MaterializedSource>(source));
}

template <typename MaterializedSource>
decltype(auto) materialized_reference(MaterializedSource &&source) {
  return materialized_source_traits_t<MaterializedSource>::reference(source);
}

struct empty_conversion_context {};

template <typename Target, typename Conversion, typename Source>
Target convert_type(Source &&source, Conversion) {
  using source_type = Source &&;
  static_assert(Conversion::available,
                "requested type conversion is unavailable");
  static_assert(type_list_size_v<typename Conversion::cache_types> == 0,
                "conversion outside resolution cannot retain its result");

  empty_conversion_context state;
  return Conversion::apply(state, state, std::forward<Source>(source),
                           describe_type<Target>(),
                           describe_type<source_type>());
}

template <typename Target, typename Access = consume, typename Source>
Target convert_type(Source &&source) {
  using source_type = Source &&;
  using conversion = type_conversion_path_t<Target, source_type, Access>;
  return convert_type<Target>(std::forward<Source>(source), conversion{});
}

template <typename T>
using resolution_object_t = std::remove_cv_t<
    std::conditional_t<std::is_pointer_v<std::remove_reference_t<T>>,
                       std::remove_pointer_t<std::remove_reference_t<T>>,
                       std::remove_reference_t<T>>>;

template <typename Target, typename Source, typename Conversion>
struct type_resolution {
private:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;

  template <typename Storage>
  using stored_type =
      std::remove_cv_t<std::remove_reference_t<typename Storage::type>>;

public:
  using source_type = Source;
  using cache_types = typename Conversion::cache_types;

  template <typename Storage>
  using temporary_types =
      std::conditional_t<!Storage::conversions::is_stable &&
                             !std::is_reference_v<Target> &&
                             !std::is_pointer_v<Target> &&
                             !std::is_same_v<target_type, stored_type<Storage>>,
                         type_list<target_type>, type_list<>>;

  static constexpr bool requires_source_retention =
      !std::is_same_v<resolution_object_t<Target>, resolution_object_t<Source>>;

  template <typename Storage, typename Resolver, typename Context,
            typename MaterializedSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, MaterializedSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    if constexpr (!Conversion::available) {
      throw make_type_not_convertible_exception(requested_type,
                                                registered_type);
    } else if constexpr (std::is_lvalue_reference_v<Source>) {
      return Conversion::apply(resolver, context,
                               materialized_reference(source), requested_type,
                               registered_type);
    } else if constexpr (std::is_rvalue_reference_v<Source>) {
      return Conversion::apply(
          resolver, context,
          static_cast<Source>(materialized_reference(source)), requested_type,
          registered_type);
    } else {
      return Conversion::apply(
          resolver, context,
          materialized_value(std::forward<MaterializedSource>(source)),
          requested_type, registered_type);
    }
  }
};
} // namespace detail
} // namespace dingo
