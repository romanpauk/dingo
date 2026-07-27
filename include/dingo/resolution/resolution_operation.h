//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/config.h>

#include <dingo/core/exceptions.h>
#include <dingo/storage/materialized_source.h>
#include <dingo/type/type_conversion_traits.h>
#include <dingo/type/type_list.h>
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

template <typename Conversion> struct type_conversion;

template <typename Target, typename Source, typename RequiredAccess,
          typename Argument>
struct type_conversion<
    traits_type_conversion<Target, Source, RequiredAccess, Argument>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &, Context &, ActualSource &&source,
                      type_descriptor, type_descriptor) {
    using source_type = std::remove_cv_t<std::remove_reference_t<ActualSource>>;
    using conversion_traits = type_conversion_traits<Target, source_type>;
    if constexpr (std::is_same_v<Argument, Source>) {
      return static_cast<Target>(
          conversion_traits::convert(std::forward<ActualSource>(source)));
    } else {
      return static_cast<Target>(
          conversion_traits::convert(static_cast<Argument>(source)));
    }
  }
};

template <typename Target, typename Source>
struct type_conversion<identity_type_conversion<Target, Source>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static decltype(auto) apply(Resolver &, Context &, ActualSource &&source,
                              type_descriptor, type_descriptor) {
    return std::forward<ActualSource>(source);
  }
};

template <typename Target, typename Source>
struct type_conversion<reference_type_conversion<Target, Source>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target &apply(Resolver &, Context &, ActualSource &&source,
                       type_descriptor, type_descriptor) {
    return static_cast<Target &>(source);
  }
};

template <typename Target, typename Source>
struct type_conversion<address_type_conversion<Target, Source>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &, Context &, ActualSource &&source,
                      type_descriptor, type_descriptor) {
    return static_cast<Target>(std::addressof(source));
  }
};

template <typename Target, typename Source>
struct type_conversion<array_reference_type_conversion<Target, Source>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &, Context &, ActualSource &&source,
                      type_descriptor, type_descriptor) {
    using array_type = std::remove_reference_t<Target>;
    return *reinterpret_cast<std::add_pointer_t<array_type>>(source);
  }
};

template <typename Target, typename Source>
struct type_conversion<array_pointer_type_conversion<Target, Source>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &, Context &, ActualSource &&source,
                      type_descriptor, type_descriptor) {
    return reinterpret_cast<Target>(source);
  }
};

template <typename Target, typename Source>
struct type_conversion<pointer_access_type_conversion<Target, Source>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &, Context &, ActualSource &&source,
                      type_descriptor, type_descriptor) {
    using source_type = std::remove_cv_t<std::remove_reference_t<ActualSource>>;
    return static_cast<Target>(type_traits<source_type>::get(source));
  }
};

template <typename Target, typename Source, typename Conversion>
struct type_conversion<
    dereference_type_conversion<Target, Source, Conversion>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, ActualSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    return type_conversion<Conversion>::apply(
        resolver, context, *std::forward<ActualSource>(source), requested_type,
        registered_type);
  }
};

template <typename Target, typename Source, typename Conversion>
struct type_conversion<borrowed_type_conversion<Target, Source, Conversion>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, ActualSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    using source_type = std::remove_cv_t<std::remove_reference_t<ActualSource>>;
    return type_conversion<Conversion>::apply(
        resolver, context,
        type_traits<source_type>::borrow(std::forward<ActualSource>(source)),
        requested_type, registered_type);
  }
};

template <typename Target, typename Source, typename Conversion>
struct type_conversion<
    wrapper_type_conversion<Target, Source, Conversion, false>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &resolver, Context &context,
                      ActualSource &&source, type_descriptor requested_type,
                      type_descriptor registered_type) {
    using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
    return type_traits<target_type>::wrap(type_conversion<Conversion>::apply(
        resolver, context, std::forward<ActualSource>(source), requested_type,
        registered_type));
  }
};

template <typename Target, typename Source, typename Conversion>
struct type_conversion<
    wrapper_type_conversion<Target, Source, Conversion, true>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &resolver, Context &context,
                      ActualSource &&source, type_descriptor requested_type,
                      type_descriptor registered_type) {
    using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
    using source_type = std::remove_cv_t<std::remove_reference_t<ActualSource>>;
    if (type_traits<source_type>::empty(source)) {
      return type_traits<target_type>::make_empty();
    }

    auto *value = type_traits<source_type>::get(source);
    if constexpr (std::is_lvalue_reference_v<ActualSource>) {
      return type_traits<target_type>::wrap(type_conversion<Conversion>::apply(
          resolver, context, *value, requested_type, registered_type));
    } else {
      return type_traits<target_type>::wrap(type_conversion<Conversion>::apply(
          resolver, context, std::move(*value), requested_type,
          registered_type));
    }
  }
};

template <typename Target, typename Source, typename Selected,
          typename Conversion>
struct type_conversion<
    target_alternative_type_conversion<Target, Source, Selected, Conversion>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &resolver, Context &context,
                      ActualSource &&source, type_descriptor requested_type,
                      type_descriptor registered_type) {
    using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
    return alternative_type_traits<target_type>::template wrap<Selected>(
        type_conversion<Conversion>::apply(resolver, context,
                                           std::forward<ActualSource>(source),
                                           requested_type, registered_type));
  }
};

template <typename Conversion>
struct type_conversion<converted_construction<Conversion>>
    : type_conversion<Conversion> {};

template <typename Target, typename Source, typename Conversion>
struct type_conversion<retained_type_conversion<Target, Source, Conversion>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &resolver, Context &context,
                      ActualSource &&source, type_descriptor requested_type,
                      type_descriptor registered_type) {
    using retained_type =
        std::remove_cv_t<std::conditional_t<std::is_pointer_v<Target>,
                                            std::remove_pointer_t<Target>,
                                            std::remove_reference_t<Target>>>;
    auto &retained = resolver.template retain_conversion<retained_type>(
        context, type_conversion<Conversion>::apply(
                     resolver, context, std::forward<ActualSource>(source),
                     requested_type, registered_type));
    if constexpr (std::is_pointer_v<Target>) {
      return std::addressof(retained);
    } else {
      return retained;
    }
  }
};

template <typename Target, typename Source, typename Branches>
struct alternative_conversion;

template <typename Target, typename Source>
struct alternative_conversion<Target, Source, type_list<>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &, Context &, ActualSource &&,
                      type_descriptor requested_type,
                      type_descriptor registered_type) {
    throw make_type_not_convertible_exception(requested_type, registered_type);
  }
};

template <typename Target, typename Source, typename Alternative,
          typename Conversion, typename... Branches>
struct alternative_conversion<
    Target, Source,
    type_list<alternative_type_conversion_branch<Alternative, Conversion>,
              Branches...>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, ActualSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    using source_type = std::remove_cv_t<std::remove_reference_t<ActualSource>>;
    if constexpr (Conversion::available) {
      if (auto *value =
              alternative_type_traits<source_type>::template get<Alternative>(
                  source)) {
        if constexpr (std::is_lvalue_reference_v<ActualSource>) {
          return type_conversion<Conversion>::apply(
              resolver, context, *value, requested_type, registered_type);
        } else {
          return type_conversion<Conversion>::apply(
              resolver, context, std::move(*value), requested_type,
              registered_type);
        }
      }
    }

    if constexpr ((Branches::conversion::available || ...)) {
      return alternative_conversion<Target, Source, type_list<Branches...>>::
          apply(resolver, context, std::forward<ActualSource>(source),
                requested_type, registered_type);
    } else {
      throw make_type_not_convertible_exception(requested_type,
                                                registered_type);
    }
  }
};

template <typename Target, typename Source, typename Branches>
struct type_conversion<alternative_type_conversion<Target, Source, Branches>>
    : alternative_conversion<Target, Source, Branches> {};

template <typename Conversion> struct conversion_cache_types {
  using type = type_list<>;
};

template <typename Target, typename Source, typename Conversion>
struct conversion_cache_types<
    borrowed_type_conversion<Target, Source, Conversion>>
    : conversion_cache_types<Conversion> {};

template <typename Target, typename Source, typename Conversion>
struct conversion_cache_types<
    dereference_type_conversion<Target, Source, Conversion>>
    : conversion_cache_types<Conversion> {};

template <typename Target, typename Source, typename Conversion,
          bool UnwrapSource>
struct conversion_cache_types<
    wrapper_type_conversion<Target, Source, Conversion, UnwrapSource>>
    : conversion_cache_types<Conversion> {};

template <typename Target, typename Source, typename Selected,
          typename Conversion>
struct conversion_cache_types<
    target_alternative_type_conversion<Target, Source, Selected, Conversion>>
    : conversion_cache_types<Conversion> {};

template <typename Conversion>
struct conversion_cache_types<converted_construction<Conversion>>
    : conversion_cache_types<Conversion> {};

template <typename Target, typename Source, typename Conversion>
struct conversion_cache_types<
    retained_type_conversion<Target, Source, Conversion>> {
private:
  using retained_type =
      std::remove_cv_t<std::conditional_t<std::is_pointer_v<Target>,
                                          std::remove_pointer_t<Target>,
                                          std::remove_reference_t<Target>>>;

public:
  using type = type_list_unique_t<
      type_list_cat_t<type_list<retained_type>,
                      typename conversion_cache_types<Conversion>::type>>;
};

template <typename Branches> struct alternative_conversion_cache_types;

template <typename... Branches>
struct alternative_conversion_cache_types<type_list<Branches...>> {
  using type = type_list_unique_t<type_list_cat_t<
      typename conversion_cache_types<typename Branches::conversion>::type...>>;
};

template <typename Target, typename Source, typename Branches>
struct conversion_cache_types<
    alternative_type_conversion<Target, Source, Branches>>
    : alternative_conversion_cache_types<Branches> {};

struct empty_conversion_context {};

template <typename Target, typename Conversion, typename Source>
Target convert_type(Source &&source, Conversion) {
  using source_type = Source &&;
  static_assert(Conversion::available,
                "requested type conversion is unavailable");
  static_assert(
      type_list_size_v<typename conversion_cache_types<Conversion>::type> == 0,
      "conversion outside resolution cannot retain its result");

  empty_conversion_context state;
  return type_conversion<Conversion>::apply(
      state, state, std::forward<Source>(source), describe_type<Target>(),
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
  template <typename Storage>
  using cache_types = typename conversion_cache_types<Conversion>::type;

  template <typename Storage>
  using temporary_types =
      std::conditional_t<!Storage::conversions::is_stable &&
                             !std::is_reference_v<Target> &&
                             !std::is_pointer_v<Target> &&
                             !std::is_same_v<target_type, stored_type<Storage>>,
                         type_list<target_type>, type_list<>>;

  template <typename Storage>
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
      return type_conversion<Conversion>::apply(
          resolver, context, materialized_reference(source), requested_type,
          registered_type);
    } else if constexpr (std::is_rvalue_reference_v<Source>) {
      return type_conversion<Conversion>::apply(
          resolver, context,
          static_cast<Source>(materialized_reference(source)), requested_type,
          registered_type);
    } else {
      return type_conversion<Conversion>::apply(
          resolver, context,
          materialized_value(std::forward<MaterializedSource>(source)),
          requested_type, registered_type);
    }
  }
};
} // namespace detail
} // namespace dingo
