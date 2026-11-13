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
#include <dingo/conversion/paths.h>
#include <dingo/conversion/source.h>
#include <dingo/type/type_traits.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {
struct empty_conversion_context {};

template <typename Target, typename Conversion, typename Source>
Target convert_type(Source &&source, Conversion) {
  using source_type = Source &&;
  static_assert(Conversion::available,
                "requested type conversion is unavailable");

  empty_conversion_context state;
  return Conversion::apply(state, state, std::forward<Source>(source),
                           detail::described_type_v<Target>,
                           detail::described_type_v<source_type>);
}

template <typename Target, typename Access = consume, typename Source>
Target convert_type(Source &&source) {
  using source_type = Source &&;
  using conversion = type_conversion_path_t<Target, source_type, Access>;
  return convert_type<Target>(std::forward<Source>(source), conversion{});
}

template <typename Source, typename Conversion> struct type_resolution {

  template <typename Resolver, typename Context, typename MaterializedSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, MaterializedSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    if constexpr (!Conversion::available) {
      throw_type_not_convertible(requested_type, registered_type);
    } else if constexpr (std::is_lvalue_reference_v<Source>) {
      return Conversion::apply(resolver, context, *source, requested_type,
                               registered_type);
    } else if constexpr (std::is_rvalue_reference_v<Source>) {
      return Conversion::apply(resolver, context, static_cast<Source>(*source),
                               requested_type, registered_type);
    } else {
      return Conversion::apply(resolver, context,
                               std::forward<MaterializedSource>(source).get(),
                               requested_type, registered_type);
    }
  }
};
} // namespace detail
} // namespace dingo
