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

template <typename Conversion> struct type_conversion;

template <typename Route> struct conversion_walk;
template <typename Conversion> struct conversion_descriptor;
template <typename Conversion>
using conversion_route_t = typename conversion_descriptor<Conversion>::steps;
template <typename Conversion>
using conversion_cache_types_t =
    typename conversion_descriptor<Conversion>::cache_types;

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

// A conversion is a small route of steps. The route builder turns the nested
// conversion type into the steps in execution order; conversion_walk applies
// them with one continuation. Leaf conversions keep the existing primitives,
// so the walker is the only code that composes them.
template <typename Route> struct conversion_walk;

template <> struct conversion_walk<type_list<>> {
  template <typename Resolver, typename Context, typename Source>
  static decltype(auto) apply(Resolver &, Context &, Source &&source,
                              type_descriptor, type_descriptor) {
    return std::forward<Source>(source);
  }
};

template <typename Step, typename... Rest>
struct conversion_walk<type_list<Step, Rest...>> {
  template <typename Resolver, typename Context, typename Source>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type) {
    using next = conversion_walk<type_list<Rest...>>;
    constexpr bool terminal = sizeof...(Rest) == 0;
    return Step::template apply<terminal>(
        resolver, context, std::forward<Source>(source), requested_type,
        registered_type, next{});
  }
};

template <typename Value> decltype(auto) forward_temporary(Value &value) {
  if constexpr (std::is_move_constructible_v<Value>) {
    return std::move(value);
  } else {
    return value;
  }
}

template <typename Conversion> struct conversion_leaf_step {
  static constexpr conversion_kind kind = Conversion::kind;

  template <bool Terminal, typename Resolver, typename Context, typename Source,
            typename Next>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type, Next) {
    if constexpr (!Conversion::available) {
      throw make_type_not_convertible_exception(requested_type,
                                                registered_type);
    } else if constexpr (Terminal) {
      return type_conversion<Conversion>::apply(
          resolver, context, std::forward<Source>(source), requested_type,
          registered_type);
    } else {
      auto &&value = type_conversion<Conversion>::apply(
          resolver, context, std::forward<Source>(source), requested_type,
          registered_type);
      return Next::apply(resolver, context,
                         std::forward<decltype(value)>(value), requested_type,
                         registered_type);
    }
  }
};

struct conversion_dereference_step {
  static constexpr conversion_kind kind = conversion_kind::dereference;

  template <bool Terminal, typename Resolver, typename Context, typename Source,
            typename Next>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type, Next) {
    if constexpr (Terminal) {
      return *std::forward<Source>(source);
    } else {
      return Next::apply(resolver, context, *std::forward<Source>(source),
                         requested_type, registered_type);
    }
  }
};

struct conversion_borrow_step {
  static constexpr conversion_kind kind = conversion_kind::borrowed;

  template <bool Terminal, typename Resolver, typename Context, typename Source,
            typename Next>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type, Next) {
    using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
    if constexpr (Terminal) {
      return type_traits<source_type>::borrow(std::forward<Source>(source));
    } else {
      return Next::apply(
          resolver, context,
          type_traits<source_type>::borrow(std::forward<Source>(source)),
          requested_type, registered_type);
    }
  }
};

template <typename Target> struct conversion_wrap_step {
  static constexpr conversion_kind kind = conversion_kind::wrapper;

  template <bool Terminal, typename Resolver, typename Context, typename Source,
            typename Next>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type, Next) {
    using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
    if constexpr (Terminal) {
      return type_traits<target_type>::wrap(std::forward<Source>(source));
    } else {
      auto wrapped =
          type_traits<target_type>::wrap(std::forward<Source>(source));
      return Next::apply(resolver, context, forward_temporary(wrapped),
                         requested_type, registered_type);
    }
  }
};

template <typename Target> struct conversion_retain_step {
  static constexpr conversion_kind kind = conversion_kind::retained;

  template <bool Terminal, typename Resolver, typename Context, typename Source,
            typename Next>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type, Next) {
    using retained_type =
        std::remove_cv_t<std::conditional_t<std::is_pointer_v<Target>,
                                            std::remove_pointer_t<Target>,
                                            std::remove_reference_t<Target>>>;
    auto &retained = resolver.template retain_conversion<retained_type>(
        context, std::forward<Source>(source));
    if constexpr (std::is_pointer_v<Target>) {
      if constexpr (Terminal) {
        return std::addressof(retained);
      } else {
        return Next::apply(resolver, context, std::addressof(retained),
                           requested_type, registered_type);
      }
    } else {
      if constexpr (Terminal) {
        return retained;
      } else {
        return Next::apply(resolver, context, retained, requested_type,
                           registered_type);
      }
    }
  }
};

template <typename Target, typename Selected>
struct conversion_target_alternative_step {
  static constexpr conversion_kind kind = conversion_kind::target_alternative;

  template <bool Terminal, typename Resolver, typename Context, typename Source,
            typename Next>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type, Next) {
    using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
    if constexpr (Terminal) {
      return alternative_type_traits<target_type>::template wrap<Selected>(
          std::forward<Source>(source));
    } else {
      auto wrapped =
          alternative_type_traits<target_type>::template wrap<Selected>(
              std::forward<Source>(source));
      return Next::apply(resolver, context, forward_temporary(wrapped),
                         requested_type, registered_type);
    }
  }
};

template <typename Target, typename InnerRoute>
struct conversion_optional_wrap_step {
  static constexpr conversion_kind kind = conversion_kind::wrapper;

  template <bool Terminal, typename Resolver, typename Context, typename Source,
            typename Next>
  static decltype(auto) apply(Resolver &resolver, Context &context,
                              Source &&source, type_descriptor requested_type,
                              type_descriptor registered_type, Next) {
    using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
    using source_type = std::remove_cv_t<std::remove_reference_t<Source>>;
    if (type_traits<source_type>::empty(source)) {
      if constexpr (Terminal) {
        return type_traits<target_type>::make_empty();
      } else {
        auto empty = type_traits<target_type>::make_empty();
        return Next::apply(resolver, context, forward_temporary(empty),
                           requested_type, registered_type);
      }
    }

    auto *value = type_traits<source_type>::get(source);
    if constexpr (std::is_lvalue_reference_v<Source>) {
      auto wrapped =
          type_traits<target_type>::wrap(conversion_walk<InnerRoute>::apply(
              resolver, context, *value, requested_type, registered_type));
      if constexpr (Terminal) {
        return wrapped;
      } else {
        return Next::apply(resolver, context, forward_temporary(wrapped),
                           requested_type, registered_type);
      }
    } else {
      auto wrapped =
          type_traits<target_type>::wrap(conversion_walk<InnerRoute>::apply(
              resolver, context, std::move(*value), requested_type,
              registered_type));
      if constexpr (Terminal) {
        return wrapped;
      } else {
        return Next::apply(resolver, context, forward_temporary(wrapped),
                           requested_type, registered_type);
      }
    }
  }
};

template <typename Target, typename Source, typename... Branches>
struct conversion_alternative_walk;

template <typename Target, typename Source>
struct conversion_alternative_walk<Target, Source> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &, Context &, ActualSource &&,
                      type_descriptor requested_type,
                      type_descriptor registered_type) {
    throw make_type_not_convertible_exception(requested_type, registered_type);
  }
};

template <typename Target, typename Source, typename Alternative,
          typename Conversion, typename... Branches>
struct conversion_alternative_walk<
    Target, Source, alternative_type_conversion_branch<Alternative, Conversion>,
    Branches...> {
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
          return conversion_walk<conversion_route_t<Conversion>>::apply(
              resolver, context, *value, requested_type, registered_type);
        } else {
          return conversion_walk<conversion_route_t<Conversion>>::apply(
              resolver, context, std::move(*value), requested_type,
              registered_type);
        }
      }
    }

    if constexpr ((Branches::conversion::available || ...)) {
      return conversion_alternative_walk<Target, Source, Branches...>::apply(
          resolver, context, std::forward<ActualSource>(source), requested_type,
          registered_type);
    } else {
      throw make_type_not_convertible_exception(requested_type,
                                                registered_type);
    }
  }
};

template <typename Target, typename Source, typename Branches>
struct conversion_alternative_step;

template <typename Target, typename Source, typename... Branches>
struct conversion_alternative_step<Target, Source, type_list<Branches...>> {
  static constexpr conversion_kind kind = conversion_kind::alternative;

  template <bool Terminal, typename Resolver, typename Context,
            typename SourceType, typename Next>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, SourceType &&source,
        type_descriptor requested_type, type_descriptor registered_type, Next) {
    if constexpr (Terminal) {
      return conversion_alternative_walk<Target, Source, Branches...>::apply(
          resolver, context, std::forward<SourceType>(source), requested_type,
          registered_type);
    } else {
      auto &&value =
          conversion_alternative_walk<Target, Source, Branches...>::apply(
              resolver, context, std::forward<SourceType>(source),
              requested_type, registered_type);
      return Next::apply(resolver, context,
                         std::forward<decltype(value)>(value), requested_type,
                         registered_type);
    }
  }
};

template <typename Branches> struct alternative_conversion_cache_types;

template <typename... Branches>
struct alternative_conversion_cache_types<type_list<Branches...>> {
  using type =
      type_list_unique_t<type_list_cat_t<typename conversion_descriptor<
          typename Branches::conversion>::cache_types...>>;
};

template <typename Conversion> struct conversion_descriptor {
  using steps = type_list<conversion_leaf_step<Conversion>>;
  using cache_types = type_list<>;
};

template <typename Target, typename Source, typename Conversion>
struct conversion_descriptor<
    dereference_type_conversion<Target, Source, Conversion>> {
  using steps = type_list_cat_t<type_list<conversion_dereference_step>,
                                conversion_route_t<Conversion>>;
  using cache_types = conversion_cache_types_t<Conversion>;
};

template <typename Target, typename Source, typename Conversion>
struct conversion_descriptor<
    borrowed_type_conversion<Target, Source, Conversion>> {
  using steps = type_list_cat_t<type_list<conversion_borrow_step>,
                                conversion_route_t<Conversion>>;
  using cache_types = conversion_cache_types_t<Conversion>;
};

template <typename Target, typename Source, typename Conversion, bool Unwrap>
struct conversion_descriptor<
    wrapper_type_conversion<Target, Source, Conversion, Unwrap>> {
  using inner = conversion_route_t<Conversion>;
  using steps = std::conditional_t<
      Unwrap, type_list<conversion_optional_wrap_step<Target, inner>>,
      type_list_cat_t<inner, type_list<conversion_wrap_step<Target>>>>;
  using cache_types = conversion_cache_types_t<Conversion>;
};

template <typename Target, typename Source, typename Selected,
          typename Conversion>
struct conversion_descriptor<
    target_alternative_type_conversion<Target, Source, Selected, Conversion>> {
  using steps = type_list_cat_t<
      conversion_route_t<Conversion>,
      type_list<conversion_target_alternative_step<Target, Selected>>>;
  using cache_types = conversion_cache_types_t<Conversion>;
};

template <typename Conversion>
struct conversion_descriptor<converted_construction<Conversion>> {
  using steps = conversion_route_t<Conversion>;
  using cache_types = conversion_cache_types_t<Conversion>;
};

template <typename Target, typename Source, typename Conversion>
struct conversion_descriptor<
    retained_type_conversion<Target, Source, Conversion>> {
private:
  using retained_type =
      std::remove_cv_t<std::conditional_t<std::is_pointer_v<Target>,
                                          std::remove_pointer_t<Target>,
                                          std::remove_reference_t<Target>>>;

public:
  using steps = type_list_cat_t<conversion_route_t<Conversion>,
                                type_list<conversion_retain_step<Target>>>;
  using cache_types =
      type_list_unique_t<type_list_cat_t<type_list<retained_type>,
                                         conversion_cache_types_t<Conversion>>>;
};

template <typename Target, typename Source, typename Branches>
struct conversion_descriptor<
    alternative_type_conversion<Target, Source, Branches>> {
  using steps =
      type_list<conversion_alternative_step<Target, Source, Branches>>;
  using cache_types =
      typename alternative_conversion_cache_types<Branches>::type;
};

template <typename Conversion>
using conversion_walk_t = conversion_walk<conversion_route_t<Conversion>>;

template <typename Target, typename Source, typename Conversion>
struct type_conversion<
    dereference_type_conversion<Target, Source, Conversion>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, ActualSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    return conversion_walk_t<dereference_type_conversion<
        Target, Source, Conversion>>::apply(resolver, context,
                                            std::forward<ActualSource>(source),
                                            requested_type, registered_type);
  }
};

template <typename Target, typename Source, typename Conversion>
struct type_conversion<borrowed_type_conversion<Target, Source, Conversion>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, ActualSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    return conversion_walk_t<borrowed_type_conversion<
        Target, Source, Conversion>>::apply(resolver, context,
                                            std::forward<ActualSource>(source),
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
    return conversion_walk_t<wrapper_type_conversion<
        Target, Source, Conversion, false>>::apply(resolver, context,
                                                   std::forward<ActualSource>(
                                                       source),
                                                   requested_type,
                                                   registered_type);
  }
};

template <typename Target, typename Source, typename Conversion>
struct type_conversion<
    wrapper_type_conversion<Target, Source, Conversion, true>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static Target apply(Resolver &resolver, Context &context,
                      ActualSource &&source, type_descriptor requested_type,
                      type_descriptor registered_type) {
    return conversion_walk_t<wrapper_type_conversion<
        Target, Source, Conversion, true>>::apply(resolver, context,
                                                  std::forward<ActualSource>(
                                                      source),
                                                  requested_type,
                                                  registered_type);
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
    return conversion_walk_t<target_alternative_type_conversion<
        Target, Source, Selected, Conversion>>::apply(resolver, context,
                                                      std::forward<
                                                          ActualSource>(source),
                                                      requested_type,
                                                      registered_type);
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
    return conversion_walk_t<retained_type_conversion<
        Target, Source, Conversion>>::apply(resolver, context,
                                            std::forward<ActualSource>(source),
                                            requested_type, registered_type);
  }
};

template <typename Target, typename Source, typename Branches>
struct type_conversion<alternative_type_conversion<Target, Source, Branches>> {
  template <typename Resolver, typename Context, typename ActualSource>
  static decltype(auto)
  apply(Resolver &resolver, Context &context, ActualSource &&source,
        type_descriptor requested_type, type_descriptor registered_type) {
    return conversion_walk_t<alternative_type_conversion<
        Target, Source, Branches>>::apply(resolver, context,
                                          std::forward<ActualSource>(source),
                                          requested_type, registered_type);
  }
};

struct empty_conversion_context {};

template <typename Target, typename Conversion, typename Source>
Target convert_type(Source &&source, Conversion) {
  using source_type = Source &&;
  static_assert(Conversion::available,
                "requested type conversion is unavailable");
  static_assert(type_list_size_v<conversion_cache_types_t<Conversion>> == 0,
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
  template <typename Storage>
  using stored_type =
      std::remove_cv_t<std::remove_reference_t<typename Storage::type>>;

public:
  using target_type = std::remove_cv_t<std::remove_reference_t<Target>>;
  using source_type = Source;
  using conversion_type = Conversion;
  using conversion_steps = conversion_route_t<Conversion>;
  using required_access = typename Conversion::required_access;
  using cache_types = conversion_cache_types_t<Conversion>;
  static constexpr conversion_kind kind = Conversion::kind;

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
