//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_selection.h>
#include <dingo/core/context_base.h>
#include <dingo/core/exceptions.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/resolution/conversion_cache.h>
#include <dingo/resolution/resolution_operation.h>
#include <dingo/resolution/runtime_binding_interface.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_descriptor.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <memory>
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

template <typename Context> struct retained_frame_scope {
  explicit retained_frame_scope(Context &context, bool active = true)
      : context_(context), active_(active) {}

  ~retained_frame_scope() {
    if (active_) {
      context_.pop_frame();
    }
  }

private:
  Context &context_;
  bool active_;
};

enum class binding_request_kind {
  value,
  lvalue_reference,
  rvalue_reference,
  pointer,
};

template <typename RTTI> struct binding_request {
  instance_request<RTTI> request;
  cache::sink cache;
  binding_request_kind kind;
};

template <typename T, typename RTTI>
constexpr binding_request<RTTI> make_binding_request(cache::sink cache = {}) {
  return {
      {RTTI::template get_type_index<request_lookup_type_t<T>>(),
       describe_type<T>()},
      cache,
      std::is_pointer_v<T>            ? binding_request_kind::pointer
      : std::is_lvalue_reference_v<T> ? binding_request_kind::lvalue_reference
      : std::is_rvalue_reference_v<T> ? binding_request_kind::rvalue_reference
                                      : binding_request_kind::value};
}

template <typename T> T convert_resolved_binding(resolved_address result) {
  using result_type = std::remove_reference_t<T>;
  using value_type = std::remove_cv_t<result_type>;

  if constexpr (std::is_lvalue_reference_v<T>) {
    return *static_cast<result_type *>(result.address);
  } else if constexpr (std::is_rvalue_reference_v<T>) {
    return std::move(*static_cast<result_type *>(result.address));
  } else if constexpr (std::is_pointer_v<T>) {
    return static_cast<T>(result.address);
  } else if constexpr (std::is_move_constructible_v<value_type> &&
                       is_copy_constructible_v<value_type>) {
    if (result.access == resolved_address::access_kind::consume) {
      return std::move(*static_cast<value_type *>(result.address));
    }
    return *static_cast<value_type *>(result.address);
  } else if constexpr (std::is_move_constructible_v<value_type>) {
    assert(result.access == resolved_address::access_kind::consume);
    return std::move(*static_cast<value_type *>(result.address));
  } else if constexpr (is_copy_constructible_v<value_type>) {
    return *static_cast<value_type *>(result.address);
  } else {
    throw make_type_not_convertible_exception(describe_type<T>(),
                                              describe_type<value_type>());
  }
}

template <typename T> T convert_resolved_binding(void *ptr) {
  return convert_resolved_binding<T>(
      {ptr, resolved_address::access_kind::borrow});
}

template <typename T, typename Instance, typename Fn>
decltype(auto) forward_resolved_binding(Instance &&instance, Fn &&fn) {
  if constexpr (std::is_lvalue_reference_v<T> || std::is_pointer_v<T>) {
    return std::forward<Fn>(fn)(instance);
  } else if constexpr (std::is_rvalue_reference_v<T>) {
    return std::forward<Fn>(fn)(
        static_cast<std::remove_reference_t<Instance> &&>(instance));
  } else {
    return std::forward<Fn>(fn)(std::forward<Instance>(instance));
  }
}

template <typename T, typename Instance, typename Fn>
decltype(auto) consume_resolved_binding(Instance &&instance, Fn &&fn) {
  if constexpr (std::is_lvalue_reference_v<T> || std::is_pointer_v<T> ||
                (!std::is_rvalue_reference_v<T> &&
                 !std::is_constructible_v<T, Instance &&>)) {
    return std::forward<Fn>(fn)(instance);
  } else {
    return std::forward<Fn>(fn)(std::forward<Instance>(instance));
  }
}

template <typename Target>
constexpr bool matches_resolution_request(type_descriptor requested_type) {
  return matches_qualification_conversion(describe_type<Target>(),
                                          requested_type);
}

template <typename Target, typename Context, typename T>
void *result_address(construction_scope scope, Context &context, T &&instance) {
  using instance_type = std::remove_reference_t<T>;

  if constexpr (std::is_pointer_v<instance_type>) {
    return const_cast<std::remove_cv_t<std::remove_pointer_t<instance_type>> *>(
        instance);
  } else if constexpr (std::is_reference_v<T>) {
    return const_cast<std::remove_cv_t<instance_type> *>(&instance);
  } else if constexpr (std::is_constructible_v<Target, T &&>) {
    return &context.template construct<Target>(scope,
                                               std::forward<T>(instance));
  } else {
    static_assert(std::is_constructible_v<Target, T &>,
                  "resolution result must construct the requested type");
    return &context.template construct<Target>(scope, instance);
  }
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_binding_source(construction_scope scope, Context &context,
                                   Storage &storage, Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;

  auto source = materialization_traits::materialize_source(scope, context,
                                                           storage, owner);
  return std::forward<Fn>(fn)(std::move(source));
}

template <typename MaterializationTraits, typename Context, typename Storage,
          typename Owner, typename = void>
struct has_context_materialize_source : std::false_type {};

template <typename MaterializationTraits, typename Context, typename Storage,
          typename Owner>
struct has_context_materialize_source<
    MaterializationTraits, Context, Storage, Owner,
    std::void_t<decltype(MaterializationTraits::materialize_source_in_context(
        std::declval<construction_scope>(), std::declval<Context &>(),
        std::declval<Storage &>(), std::declval<Owner &>()))>>
    : std::true_type {};

template <typename Storage, typename Context, typename Owner>
inline constexpr bool materializes_value_source_v = [] {
  using source_result = decltype(std::declval<Storage &>().resolve(
      std::declval<construction_scope>(), std::declval<Context &>(),
      std::declval<Owner &>()));
  using source_type = std::remove_cv_t<std::remove_reference_t<source_result>>;
  return !std::is_reference_v<source_result> && !std::is_pointer_v<source_type>;
}();

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_tracked_binding_source(construction_scope scope,
                                           Context &context, Storage &storage,
                                           Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  auto source = materialization_traits::materialize_source(scope, context,
                                                           storage, owner);
  return std::forward<Fn>(fn)(std::move(source));
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_context_lvalue_source(construction_scope scope,
                                          Context &context, Storage &storage,
                                          Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;
  using source_type =
      std::remove_cv_t<std::remove_reference_t<decltype(storage.resolve(
          scope, context, owner))>>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  auto &source_value = context.template construct<source_type>(
      scope, storage.resolve(scope, context, owner));
  auto source = lvalue_source<source_type>(source_value);
  return std::forward<Fn>(fn)(source);
}

template <typename Storage, typename Context, typename Owner, typename Frame,
          typename Fn>
decltype(auto) with_retained_binding_source(construction_scope scope,
                                            Context &context, Storage &storage,
                                            Owner &owner, Frame &frame,
                                            Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  if (materialization_traits::retains_source(storage)) {
    const auto pushed = !context.contains_frame(&frame);
    if (pushed) {
      context.push_frame(&frame);
    }
    retained_frame_scope<Context> frame_scope(context, pushed);
    auto source = materialization_traits::materialize_source(
        persistent_scope, context, storage, owner);
    return std::forward<Fn>(fn)(std::move(source));
  }

  auto source = materialization_traits::materialize_source(scope, context,
                                                           storage, owner);
  return std::forward<Fn>(fn)(std::move(source));
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) with_runtime_binding_source(construction_scope scope,
                                           Context &context, Storage &storage,
                                           Owner &owner, Fn &&fn) {
  using materialization_traits =
      storage_materialization_traits<typename Storage::tag_type,
                                     typename Storage::type>;
  using leaf_type = leaf_type_t<typename Storage::type>;

  [[maybe_unused]] auto guard =
      materialization_traits::template make_guard<leaf_type>(context, storage);
  if constexpr (has_context_materialize_source<materialization_traits, Context,
                                               Storage, Owner>::value) {
    const bool retains_source =
        materialization_traits::retains_source(storage) ||
        scope.is_persistent();
    auto &source = [&]() -> auto & {
      if (retains_source) {
        return materialization_traits::materialize_source_in_context(
            persistent_scope, context, storage, owner);
      } else {
        return materialization_traits::materialize_source_in_context(
            scope, context, storage, owner);
      }
    }();
    if (retains_source) {
      return std::forward<Fn>(fn)(source);
    }
    return std::forward<Fn>(fn)(std::move(source));
  } else {
    auto source = [&] {
      if (materialization_traits::retains_source(storage)) {
        return materialization_traits::materialize_source(
            persistent_scope, context, storage, owner);
      } else {
        return materialization_traits::materialize_source(scope, context,
                                                          storage, owner);
      }
    }();
    return std::forward<Fn>(fn)(std::move(source));
  }
}

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

template <typename Target, typename Resolver, typename Context,
          typename MaterializedSource>
decltype(auto) resolve_binding_value(Resolver &resolver, Context &context,
                                     MaterializedSource &&source) {
  using source_type = decltype(std::forward<MaterializedSource>(source).get());
  using conversion = type_conversion_path_t<Target &, source_type, borrow>;
  static_assert(conversion::available);
  return type_conversion<conversion>::apply(
      resolver, context, std::forward<MaterializedSource>(source).get(),
      describe_type<Target>(),
      describe_type<std::remove_reference_t<source_type>>());
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename MaterializedSource>
decltype(auto) apply_resolution(Resolver &resolver, Context &context,
                                MaterializedSource &&source) {
  using operation = typename Resolution::operation;
  return operation::template apply<Storage>(
      resolver, context, std::forward<MaterializedSource>(source),
      describe_type<Request>(), describe_type<typename Storage::type>());
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename MaterializedSource,
          typename Fn>
decltype(auto) apply_consumed_resolution(Resolver &resolver, Context &context,
                                         MaterializedSource &&source, Fn &&fn) {
  auto &&instance = apply_resolution<Request, Resolution, Storage>(
      resolver, context, std::forward<MaterializedSource>(source));
  return consume_resolved_binding<Request>(
      std::forward<decltype(instance)>(instance), std::forward<Fn>(fn));
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename Owner, typename Fn>
decltype(auto) consume_runtime_binding_resolution_request(
    construction_scope scope, Context &context, Storage &storage, Owner &owner,
    Resolver &resolver, Fn &&fn) {
  return with_runtime_binding_source(
      scope, context, storage, owner, [&](auto &&source) -> decltype(auto) {
        return apply_consumed_resolution<Request, Resolution, Storage>(
            resolver, context, std::forward<decltype(source)>(source),
            std::forward<Fn>(fn));
      });
}

template <typename Request, typename Resolution, typename Storage,
          typename Resolver, typename Context, typename Owner, typename Frame,
          typename Fn>
decltype(auto)
consume_binding_resolution_request(construction_scope scope, Context &context,
                                   Storage &storage, Owner &owner, Frame &frame,
                                   Resolver &resolver, Fn &&fn) {
  auto consume = [&](auto &&source) -> decltype(auto) {
    return apply_consumed_resolution<Request, Resolution, Storage>(
        resolver, context, std::forward<decltype(source)>(source),
        std::forward<Fn>(fn));
  };
  using operation = typename Resolution::operation;
  if constexpr (operation_requires_source_retention_v<operation, Storage>) {
    return with_retained_binding_source(scope, context, storage, owner, frame,
                                        std::move(consume));
  } else {
    return with_binding_source(scope, context, storage, owner,
                               std::move(consume));
  }
}

template <typename Target, typename Context, typename Instance>
resolved_address resolve_binding_address_from_instance(construction_scope scope,
                                                       Context &context,
                                                       Instance &&instance) {
  constexpr bool owns_value =
      !std::is_reference_v<Instance> &&
      !std::is_pointer_v<std::remove_reference_t<Instance>>;
  return {detail::result_address<Target>(
              scope, context, std::forward<decltype(instance)>(instance)),
          owns_value ? resolved_address::access_kind::consume
                     : resolved_address::access_kind::borrow};
}

template <typename Resolution, typename Storage, typename Resolver,
          typename Context, typename MaterializedSource>
resolved_address
resolve_address_from_source(construction_scope scope, Resolver &resolver,
                            Context &context, MaterializedSource &&source,
                            type_descriptor requested_type,
                            type_descriptor registered_type) {
  using address_type = typename Resolution::result_type;
  using operation = typename Resolution::operation;
  auto &&instance = operation::template apply<Storage>(
      resolver, context, std::forward<MaterializedSource>(source),
      requested_type, registered_type);
  return resolve_binding_address_from_instance<address_type>(
      scope, context, std::forward<decltype(instance)>(instance));
}

template <typename RTTI, typename Binding, typename Context>
resolved_address
dispatch_binding_request(construction_scope scope, Binding &binding,
                         Context &context,
                         const binding_request<RTTI> &request) {
  switch (request.kind) {
  case binding_request_kind::pointer:
    return {binding.get_pointer(scope, context, request.request, request.cache),
            resolved_address::access_kind::borrow};
  case binding_request_kind::lvalue_reference:
    return {binding.get_lvalue_reference(scope, context, request.request,
                                         request.cache),
            resolved_address::access_kind::borrow};
  case binding_request_kind::rvalue_reference:
    return {binding.get_rvalue_reference(scope, context, request.request,
                                         request.cache),
            resolved_address::access_kind::consume};
  case binding_request_kind::value:
    return binding.get_value(scope, context, request.request, request.cache);
  }

  assert(false);
  return {};
}

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, detail::cache::sink cache = {}) {
  auto request = detail::make_binding_request<T, RTTI>(cache);
  auto result =
      detail::dispatch_binding_request<RTTI>(scope, binding, context, request);
  return detail::convert_resolved_binding<T>(result);
}

template <typename Factory, typename Context, typename... Resolutions>
resolved_address resolve_request_address(construction_scope scope,
                                         Factory &factory, Context &context,
                                         type_list<Resolutions...>,
                                         type_descriptor requested_type,
                                         type_descriptor registered_type) {
  resolved_address result{};
  (void)scope;
  const bool matched =
      ((detail::matches_resolution_request<typename Resolutions::target_type>(
            requested_type)
            ? (result = factory.template resolve_address<Resolutions>(
                   scope, context, requested_type, registered_type),
               true)
            : false) ||
       ...);

  if (!matched) {
    throw detail::make_type_not_convertible_exception(requested_type,
                                                      registered_type, context);
  }

  return result;
}

template <typename Request>
inline constexpr bool can_construct_from_normalized_request_v =
    type_traits<std::decay_t<Request>>::enabled &&
    !std::is_pointer_v<std::decay_t<Request>> &&
    !std::is_same_v<normalized_type_t<Request>, std::decay_t<Request>>;

// Rvalue requests are not "construct me a normalized value" requests. They
// are asking the routed storage to publish a movable result, so they must
// reject normalized/wrapper construction instead of silently degrading to `T`.
template <typename Request>
inline constexpr bool rvalue_request_requires_explicit_conversion_v =
    std::is_rvalue_reference_v<Request>;

template <typename Request>
inline constexpr bool construct_normalized_request_v =
    can_construct_from_normalized_request_v<Request> &&
    !rvalue_request_requires_explicit_conversion_v<Request> &&
    std::is_object_v<normalized_type_t<Request>> &&
    !std::is_abstract_v<normalized_type_t<Request>>;

template <typename Request,
          typename ResolvedRequest =
              typename request_type<Request, true>::result_type>
inline constexpr bool construct_factory_request_v =
    !rvalue_request_requires_explicit_conversion_v<Request> &&
    std::is_object_v<normalized_type_t<Request>> &&
    !std::is_abstract_v<normalized_type_t<Request>> &&
    (std::is_pointer_v<ResolvedRequest> ||
     (type_traits<ResolvedRequest>::enabled &&
      !std::is_reference_v<ResolvedRequest>) ||
     std::is_constructible_v<ResolvedRequest, normalized_type_t<Request>>);

template <typename Request, typename MakeNotConvertible, typename MakeNotFound>
[[noreturn]] void
throw_missing_rvalue_conversion(bool has_normalized_request,
                                MakeNotConvertible &&make_not_convertible,
                                MakeNotFound &&make_not_found) {
  static_assert(rvalue_request_requires_explicit_conversion_v<Request>);

  if (has_normalized_request) {
    throw std::forward<MakeNotConvertible>(make_not_convertible)();
  }

  throw std::forward<MakeNotFound>(make_not_found)();
}

template <typename Request, typename Context>
[[noreturn]] void throw_missing_rvalue_conversion(bool has_normalized_request,
                                                  Context &context) {
  using normalized_request_type = normalized_type_t<Request>;
  throw_missing_rvalue_conversion<Request>(
      has_normalized_request,
      [&]() {
        return detail::make_type_not_convertible_exception(
            describe_type<Request>(), describe_type<normalized_request_type>(),
            context);
      },
      [&]() {
        return detail::make_type_not_found_exception<Request>(context);
      });
}

template <typename Request>
[[noreturn]] void throw_missing_rvalue_conversion(bool has_normalized_request) {
  using normalized_request_type = normalized_type_t<Request>;
  throw_missing_rvalue_conversion<Request>(
      has_normalized_request,
      [&]() {
        return detail::make_type_not_convertible_exception(
            describe_type<Request>(), describe_type<normalized_request_type>());
      },
      [&]() { return detail::make_type_not_found_exception<Request>(); });
}

template <typename Request, typename ResolveExact, typename ResolveNormalized>
typename request_type<Request, true>::result_type
construct_resolved_request(ResolveExact &&resolve_exact,
                           ResolveNormalized &&resolve_normalized) {
  try {
    return std::forward<ResolveExact>(resolve_exact)();
  } catch (const type_not_convertible_exception &) {
    if constexpr (can_construct_from_normalized_request_v<Request>) {
      auto &&value = std::forward<ResolveNormalized>(resolve_normalized)();
      return type_traits<std::decay_t<Request>>::make(
          std::forward<decltype(value)>(value));
    } else {
      throw;
    }
  }
}
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
