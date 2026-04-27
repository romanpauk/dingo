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
#include <dingo/resolution/runtime_binding_interface.h>
#include <dingo/resolution/type_conversion.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_descriptor.h>
#include <dingo/type/type_traits.h>

#include <cassert>
#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Storage, typename T, typename Context, typename Source,
          typename = void>
struct has_storage_resolve_conversion : std::false_type {};

template <typename Storage, typename T, typename Context, typename Source>
struct has_storage_resolve_conversion<
    Storage, T, Context, Source,
    std::void_t<
        decltype(std::declval<Storage&>().template resolve_conversion<T>(
            std::declval<Context&>(), std::declval<Source>()))>>
    : std::true_type {};

template <bool Enabled, typename ConversionTypes>
struct binding_conversion_cache_base;

template <typename ConversionTypes>
struct binding_conversion_cache_base<true, ConversionTypes> {
    template <typename T, typename Context, typename... Args>
    T& construct_conversion(Context&, Args&&... args) {
        return conversions_.template construct<T>(std::forward<Args>(args)...);
    }

  protected:
    conversion_cache<ConversionTypes> conversions_;
};

template <typename ConversionTypes>
struct binding_conversion_cache_base<false, ConversionTypes> {
    template <typename T, typename Context, typename... Args>
    T& construct_conversion(Context& context, Args&&... args) {
        return context.template construct<T>(std::forward<Args>(args)...);
    }
};

template <typename Context> struct preserve_closure_scope {
    explicit preserve_closure_scope(Context& context)
        : context_(context), exceptions_(std::uncaught_exceptions()) {}

    ~preserve_closure_scope() {
        if (std::uncaught_exceptions() == exceptions_) {
            context_.pop();
        }
    }

  private:
    Context& context_;
    int exceptions_;
};

template <typename T, typename = void>
struct has_type_copy_on_resolve : std::false_type {};

template <typename T>
struct has_type_copy_on_resolve<T, std::void_t<decltype(T::copy_on_resolve)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_wrapper_copy_on_resolve : std::false_type {};

template <typename T>
struct has_wrapper_copy_on_resolve<
    T, std::void_t<decltype(type_traits<T>::copy_on_resolve)>>
    : std::true_type {};

template <typename T, typename = void> struct resolved_type_traits {
    static constexpr bool copy_on_resolve = [] {
        if constexpr (has_type_copy_on_resolve<T>::value) {
            return T::copy_on_resolve;
        } else if constexpr (has_wrapper_copy_on_resolve<T>::value) {
            return type_traits<T>::copy_on_resolve;
        } else {
            return std::is_copy_constructible_v<T>;
        }
    }();
};

template <typename T>
struct resolved_type_traits<
    T, std::enable_if_t<collection_traits<T>::is_collection>> {
    static constexpr bool copy_on_resolve = resolved_type_traits<
        typename collection_traits<T>::resolve_type>::copy_on_resolve;
};

template <typename T>
inline constexpr bool copy_on_resolve_v =
    resolved_type_traits<T>::copy_on_resolve;

enum class binding_request_kind {
    value,
    lvalue_reference,
    rvalue_reference,
    pointer,
};

template <typename RTTI> struct binding_request {
    instance_request<RTTI> request;
    instance_cache_sink cache;
    binding_request_kind kind;
};

template <typename T, typename RTTI>
constexpr binding_request<RTTI>
make_binding_request(instance_cache_sink cache = {}) {
    return {
        {RTTI::template get_type_index<request_lookup_type_t<T>>(),
         describe_type<T>()},
        cache,
        std::is_pointer_v<T>            ? binding_request_kind::pointer
        : std::is_lvalue_reference_v<T> ? binding_request_kind::lvalue_reference
        : std::is_rvalue_reference_v<T> ? binding_request_kind::rvalue_reference
                                        : binding_request_kind::value};
}

template <typename T> T convert_resolved_binding(void* ptr) {
    using result_type = std::remove_reference_t<T>;

    if constexpr (std::is_lvalue_reference_v<T>) {
        return *static_cast<result_type*>(ptr);
    } else if constexpr (std::is_rvalue_reference_v<T>) {
        return std::move(*static_cast<result_type*>(ptr));
    } else if constexpr (std::is_pointer_v<T>) {
        return static_cast<T>(ptr);
    } else if constexpr (copy_on_resolve_v<T>) {
        return *static_cast<T*>(ptr);
    } else {
        return std::move(*static_cast<T*>(ptr));
    }
}

template <typename T, typename Instance, typename Fn>
decltype(auto) forward_resolved_binding(Instance&& instance, Fn&& fn) {
    if constexpr (std::is_lvalue_reference_v<T>) {
        return std::forward<Fn>(fn)(instance);
    } else if constexpr (std::is_rvalue_reference_v<T>) {
        return std::forward<Fn>(fn)(std::move(instance));
    } else if constexpr (std::is_pointer_v<T>) {
        return std::forward<Fn>(fn)(instance);
    } else {
        return std::forward<Fn>(fn)(std::forward<Instance>(instance));
    }
}

template <typename T, typename Instance, typename Fn>
decltype(auto) consume_resolved_binding(Instance&& instance, Fn&& fn) {
    if constexpr (std::is_lvalue_reference_v<T>) {
        return std::forward<Fn>(fn)(instance);
    } else if constexpr (std::is_rvalue_reference_v<T>) {
        return std::forward<Fn>(fn)(std::move(instance));
    } else if constexpr (std::is_pointer_v<T>) {
        return std::forward<Fn>(fn)(instance);
    } else if constexpr (copy_on_resolve_v<T>) {
        using value_type = std::remove_reference_t<T>;
        return std::forward<Fn>(fn)(
            value_type(std::forward<Instance>(instance)));
    } else {
        return std::forward<Fn>(fn)(std::move(instance));
    }
}

template <typename ExactLookup>
constexpr bool matches_exact_lookup(type_descriptor requested_type) {
    if (requested_type == describe_type<ExactLookup>()) {
        return true;
    }

    if constexpr (std::is_lvalue_reference_v<ExactLookup>) {
        using target_type = std::remove_reference_t<ExactLookup>;
        return requested_type == describe_type<const target_type&>();
    } else if constexpr (std::is_pointer_v<ExactLookup>) {
        using target_type = std::remove_pointer_t<ExactLookup>;
        return requested_type == describe_type<const target_type*>();
    } else {
        return false;
    }
}

template <typename Target, typename Context, typename T>
void* get_address_as(Context& context, T&& instance) {
    using instance_type = std::remove_reference_t<T>;

    if constexpr (std::is_pointer_v<instance_type>) {
        return instance;
    } else if constexpr (std::is_reference_v<T>) {
        return &instance;
    } else {
        return &context.template construct<Target>(std::forward<T>(instance));
    }
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) materialize_binding_source(Context& context, Storage& storage,
                                          Owner& owner, Fn&& fn) {
    using materialization_traits =
        storage_materialization_traits<typename Storage::tag_type,
                                       typename Storage::type>;

    auto source =
        materialization_traits::materialize_source(context, storage, owner);
    return std::forward<Fn>(fn)(std::move(source));
}

template <typename Storage, typename Context, typename Owner, typename Fn>
decltype(auto) materialize_tracked_binding_source(Context& context,
                                                  Storage& storage,
                                                  Owner& owner, Fn&& fn) {
    using materialization_traits =
        storage_materialization_traits<typename Storage::tag_type,
                                       typename Storage::type>;
    using leaf_type = leaf_type_t<typename Storage::type>;

    [[maybe_unused]] auto guard =
        materialization_traits::template make_guard<leaf_type>(context,
                                                               storage);
    auto source =
        materialization_traits::materialize_source(context, storage, owner);
    return std::forward<Fn>(fn)(std::move(source));
}

template <typename Storage, typename Context, typename Owner, typename Closure,
          typename Fn>
decltype(auto)
materialize_binding_resolution_source(Context& context, Storage& storage,
                                      Owner& owner, Closure& closure, Fn&& fn) {
    using materialization_traits =
        storage_materialization_traits<typename Storage::tag_type,
                                       typename Storage::type>;
    using leaf_type = leaf_type_t<typename Storage::type>;

    [[maybe_unused]] auto guard =
        materialization_traits::template make_guard<leaf_type>(context,
                                                               storage);
    if (materialization_traits::preserves_closure(storage)) {
        context.push(&closure);
        preserve_closure_scope<Context> closure_scope(context);
        auto source =
            materialization_traits::materialize_source(context, storage, owner);
        return std::forward<Fn>(fn)(std::move(source));
    }

    auto source =
        materialization_traits::materialize_source(context, storage, owner);
    return std::forward<Fn>(fn)(std::move(source));
}

template <typename T, typename Storage, typename ConversionResolver,
          typename Context, typename Source>
decltype(auto) resolve_binding_conversion(Storage& storage,
                                          ConversionResolver& resolver,
                                          Context& context, Source&& source) {
    if constexpr (has_storage_resolve_conversion<Storage, T, Context,
                                                 Source&&>::value) {
        return storage.template resolve_conversion<T>(
            context, std::forward<Source>(source));
    } else {
        return resolver.template construct_conversion<T>(
            context, std::forward<Source>(source));
    }
}

template <typename Target, typename Resolver, typename Context,
          typename SourceCapability>
decltype(auto) resolve_binding_value(Resolver& resolver, Context& context,
                                     SourceCapability&& source) {
    using source_capability = std::remove_reference_t<SourceCapability>;
    using source_type = decltype(std::declval<source_capability>().get());

    if constexpr (std::is_same_v<Target, source_type>) {
        return std::forward<SourceCapability>(source).get();
    } else {
        return resolver.template resolve_conversion<Target>(
            context, std::forward<SourceCapability>(source).get());
    }
}

template <typename Request, typename Storage, typename Resolver,
          typename Context, typename SourceCapability>
decltype(auto) resolve_binding_request(Resolver& resolver, Context& context,
                                       SourceCapability&& source) {
    using source_capability = std::remove_reference_t<SourceCapability>;
    using source_type = decltype(std::declval<source_capability>().get());

    if constexpr (std::is_same_v<Request, source_type>) {
        return std::forward<SourceCapability>(source).get();
    } else {
        return type_conversion<Request, source_capability>::apply(
            resolver, context, std::forward<SourceCapability>(source),
            describe_type<Request>(), describe_type<typename Storage::type>());
    }
}

template <typename Request, typename Storage, typename Resolver,
          typename Context, typename SourceCapability, typename Fn>
decltype(auto)
handle_resolved_binding_request(Resolver& resolver, Context& context,
                                SourceCapability&& source, Fn&& fn) {
    auto&& instance = resolve_binding_request<Request, Storage>(
        resolver, context, std::forward<SourceCapability>(source));
    return forward_resolved_binding<Request>(
        std::forward<decltype(instance)>(instance), std::forward<Fn>(fn));
}

template <typename Request, typename Storage, typename Resolver,
          typename Context, typename SourceCapability, typename Fn>
decltype(auto)
consume_resolved_binding_request(Resolver& resolver, Context& context,
                                 SourceCapability&& source, Fn&& fn) {
    auto&& instance = resolve_binding_request<Request, Storage>(
        resolver, context, std::forward<SourceCapability>(source));
    return consume_resolved_binding<Request>(
        std::forward<decltype(instance)>(instance), std::forward<Fn>(fn));
}

template <typename Request, typename Storage, typename Resolver,
          typename Context, typename Owner, typename Fn>
decltype(auto) forward_binding_request(Context& context, Storage& storage,
                                       Owner& owner, Resolver& resolver,
                                       Fn&& fn) {
    return materialize_tracked_binding_source(
        context, storage, owner, [&](auto&& source) -> decltype(auto) {
            return handle_resolved_binding_request<Request, Storage>(
                resolver, context, std::forward<decltype(source)>(source),
                std::forward<Fn>(fn));
        });
}

template <typename Request, typename Storage, typename Resolver,
          typename Context, typename Owner, typename Closure, typename Fn>
decltype(auto)
forward_binding_resolution_request(Context& context, Storage& storage,
                                   Owner& owner, Closure& closure,
                                   Resolver& resolver, Fn&& fn) {
    return materialize_binding_resolution_source(
        context, storage, owner, closure, [&](auto&& source) -> decltype(auto) {
            return handle_resolved_binding_request<Request, Storage>(
                resolver, context, std::forward<decltype(source)>(source),
                std::forward<Fn>(fn));
        });
}

template <typename Request, typename Storage, typename Resolver,
          typename Context, typename Owner, typename Fn>
decltype(auto) consume_binding_request(Context& context, Storage& storage,
                                       Owner& owner, Resolver& resolver,
                                       Fn&& fn) {
    return materialize_tracked_binding_source(
        context, storage, owner, [&](auto&& source) -> decltype(auto) {
            return consume_resolved_binding_request<Request, Storage>(
                resolver, context, std::forward<decltype(source)>(source),
                std::forward<Fn>(fn));
        });
}

template <typename Request, typename Storage, typename Resolver,
          typename Context, typename Owner, typename Closure, typename Fn>
decltype(auto)
consume_binding_resolution_request(Context& context, Storage& storage,
                                   Owner& owner, Closure& closure,
                                   Resolver& resolver, Fn&& fn) {
    return materialize_binding_resolution_source(
        context, storage, owner, closure, [&](auto&& source) -> decltype(auto) {
            return consume_resolved_binding_request<Request, Storage>(
                resolver, context, std::forward<decltype(source)>(source),
                std::forward<Fn>(fn));
        });
}

template <typename Target, typename Resolver, typename Context,
          typename SourceCapability>
decltype(auto) apply_binding_conversion_from_source(
    Resolver& resolver, Context& context, SourceCapability&& source,
    type_descriptor requested_type, type_descriptor registered_type) {
    using source_capability = std::remove_reference_t<SourceCapability>;
    return type_conversion<Target, source_capability>::apply(
        resolver, context, std::forward<SourceCapability>(source),
        requested_type, registered_type);
}

template <typename Target, typename Context, typename Instance>
void* resolve_binding_address_from_instance(Context& context,
                                            Instance&& instance) {
    return detail::get_address_as<Target>(
        context, std::forward<decltype(instance)>(instance));
}

template <typename Target, typename Resolver, typename Context,
          typename SourceCapability>
void* resolve_binding_address_from_source(Resolver& resolver, Context& context,
                                          SourceCapability&& source,
                                          type_descriptor requested_type,
                                          type_descriptor registered_type) {
    return resolve_binding_address_from_instance<Target>(
        context, apply_binding_conversion_from_source<Target>(
                     resolver, context, std::forward<SourceCapability>(source),
                     requested_type, registered_type));
}

template <typename Target, typename Request, typename Registered,
          typename Resolver, typename Context, typename SourceCapability>
void* resolve_static_binding_address_from_source(Resolver& resolver,
                                                 Context& context,
                                                 SourceCapability&& source) {
    return resolve_binding_address_from_instance<Target>(
        context, apply_binding_conversion_from_source<Target>(
                     resolver, context, std::forward<SourceCapability>(source),
                     describe_type<Request>(), describe_type<Registered>()));
}

template <typename RTTI, typename Binding, typename Context>
void* dispatch_binding_request(Binding& binding, Context& context,
                               const binding_request<RTTI>& request) {
    switch (request.kind) {
    case binding_request_kind::pointer:
        return binding.get_pointer(context, request.request, request.cache);
    case binding_request_kind::lvalue_reference:
        return binding.get_lvalue_reference(context, request.request,
                                            request.cache);
    case binding_request_kind::rvalue_reference:
        return binding.get_rvalue_reference(context, request.request,
                                            request.cache);
    case binding_request_kind::value:
        return binding.get_value(context, request.request, request.cache);
    }

    assert(false);
    return nullptr;
}

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(Binding& binding, Context& context,
                          instance_cache_sink cache = {}) {
    auto request = detail::make_binding_request<T, RTTI>(cache);
    void* ptr =
        detail::dispatch_binding_request<RTTI>(binding, context, request);
    return detail::convert_resolved_binding<T>(ptr);
}

template <typename RTTI, typename Factory, typename Context, typename... Types>
void* resolve_binding_capability_address(Factory& factory, Context& context,
                                         type_list<Types...>,
                                         const typename RTTI::type_index& type,
                                         type_descriptor requested_type,
                                         type_descriptor registered_type) {
    void* address = nullptr;
    const bool matched =
        ((RTTI::template get_type_index<lookup_type_t<Types>>() == type
              ? (address = factory.template resolve_address<Types>(
                     context, requested_type, registered_type),
                 true)
              : false) ||
         ...);

    if (!matched) {
        throw detail::make_type_not_convertible_exception(
            requested_type, registered_type, context);
    }

    return address;
}

template <typename Request>
inline constexpr bool can_wrap_normalized_request_v =
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
    can_wrap_normalized_request_v<Request> &&
    !rvalue_request_requires_explicit_conversion_v<Request> &&
    std::is_object_v<normalized_type_t<Request>> &&
    !std::is_abstract_v<normalized_type_t<Request>>;

template <typename Request,
          typename ResolvedRequest =
              typename annotated_traits<std::conditional_t<
                  rvalue_request_requires_explicit_conversion_v<Request>,
                  std::remove_reference_t<Request>, Request>>::type>
inline constexpr bool construct_factory_request_v =
    !rvalue_request_requires_explicit_conversion_v<Request> &&
    std::is_object_v<normalized_type_t<Request>> &&
    !std::is_abstract_v<normalized_type_t<Request>> &&
    (std::is_pointer_v<ResolvedRequest> ||
     (type_traits<ResolvedRequest>::enabled &&
      !std::is_reference_v<ResolvedRequest>) ||
     construction_traits<ResolvedRequest,
                         normalized_type_t<Request>>::enabled ||
     std::is_constructible_v<ResolvedRequest, normalized_type_t<Request>>);

template <typename Request, typename MakeNotConvertible, typename MakeNotFound>
[[noreturn]] void
throw_missing_rvalue_conversion(bool has_normalized_request,
                                MakeNotConvertible&& make_not_convertible,
                                MakeNotFound&& make_not_found) {
    static_assert(rvalue_request_requires_explicit_conversion_v<Request>);

    if (has_normalized_request) {
        throw std::forward<MakeNotConvertible>(make_not_convertible)();
    }

    throw std::forward<MakeNotFound>(make_not_found)();
}

template <typename Request, typename Context>
[[noreturn]] void throw_missing_rvalue_conversion(bool has_normalized_request,
                                                  Context& context) {
    using normalized_request_type = normalized_type_t<Request>;
    throw_missing_rvalue_conversion<Request>(
        has_normalized_request,
        [&]() {
            return detail::make_type_not_convertible_exception(
                describe_type<Request>(),
                describe_type<normalized_request_type>(), context);
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
                describe_type<Request>(),
                describe_type<normalized_request_type>());
        },
        [&]() { return detail::make_type_not_found_exception<Request>(); });
}

template <typename Request, typename ResolveExact, typename ResolveNormalized>
typename annotated_traits<
    std::conditional_t<std::is_rvalue_reference_v<Request>,
                       std::remove_reference_t<Request>, Request>>::type
construct_request_or_wrap_normalized(ResolveExact&& resolve_exact,
                                     ResolveNormalized&& resolve_normalized) {
    try {
        return std::forward<ResolveExact>(resolve_exact)();
    } catch (const type_not_convertible_exception&) {
        if constexpr (can_wrap_normalized_request_v<Request>) {
            auto&& value =
                std::forward<ResolveNormalized>(resolve_normalized)();
            return type_traits<std::decay_t<Request>>::make(
                std::forward<decltype(value)>(value));
        } else {
            throw;
        }
    }
}
} // namespace dingo
