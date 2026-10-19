//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/construction_scope.h>
#include <dingo/context/conversion_cache.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename Storage, typename T, typename Context, typename Source,
          typename = void>
struct has_storage_resolve_conversion : std::false_type {};

template <typename Storage, typename T, typename Context, typename Source>
struct has_storage_resolve_conversion<
    Storage, T, Context, Source,
    std::void_t<
        decltype(std::declval<Storage &>().template resolve_conversion<T>(
            std::declval<Context &>(), std::declval<Source>()))>>
    : std::true_type {};

template <bool Enabled, typename ConversionTypes>
struct binding_conversion_cache_base;

template <typename ConversionTypes>
struct binding_conversion_cache_base<true, ConversionTypes> {
  template <typename T, typename Context, typename... Args>
  T &construct_conversion(construction_scope scope, Context &context,
                          Args &&...args) {
    return conversions_.template construct<T>(scope, context,
                                              std::forward<Args>(args)...);
  }

  template <typename T, typename Owner, typename Context, typename... Args>
  T &construct_conversion_with(Owner &owner, construction_scope,
                               Context &context, Args &&...args) {
    (void)owner;
    return conversions_.template construct_tracked<T>(
        context, std::forward<Args>(args)...);
  }

  void reset_conversions() { conversions_.reset(); }

protected:
  conversion_cache<ConversionTypes> conversions_;
};

template <typename ConversionTypes>
struct binding_conversion_cache_base<false, ConversionTypes> {
  template <typename T, typename Context, typename... Args>
  T &construct_conversion(construction_scope scope, Context &context,
                          Args &&...args) {
    return context.template construct<T>(scope, std::forward<Args>(args)...);
  }

  template <typename T, typename Owner, typename Context, typename... Args>
  T &construct_conversion_with(Owner &, construction_scope scope,
                               Context &context, Args &&...args) {
    return construct_conversion<T>(scope, context, std::forward<Args>(args)...);
  }

  void reset_conversions() {}
};

template <typename T, typename Storage, typename ConversionResolver,
          typename Context, typename Source>
decltype(auto) retain_binding_conversion(construction_scope scope,
                                         Storage &storage,
                                         ConversionResolver &resolver,
                                         Context &context, Source &&source) {
  if constexpr (has_storage_resolve_conversion<Storage, T, Context,
                                               Source &&>::value) {
    return storage.template resolve_conversion<T>(context,
                                                  std::forward<Source>(source));
  } else {
    return resolver.template construct_conversion<T>(
        scope, context, std::forward<Source>(source));
  }
}

} // namespace detail
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
