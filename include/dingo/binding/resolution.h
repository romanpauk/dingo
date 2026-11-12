//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/interface.h>
#include <dingo/binding/selection.h>
#include <dingo/context/resolution.h>
#include <dingo/conversion/apply.h>
#include <dingo/request/composition.h>
#include <dingo/request/decomposition.h>
#include <dingo/request/derivation.h>
#include <dingo/storage/retention.h>
#include <dingo/storage/type_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/shape.h>
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
    throw_type_not_convertible(detail::described_type_v<T>,
                               detail::described_type_v<value_type>);
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

template <typename Resolution>
constexpr bool matches_resolution_request(type_descriptor requested_type) {
  return matches_qualification_conversion(
      detail::described_type_v<typename Resolution::target_type>,
      requested_type);
}

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, const void *cache_key = nullptr);

namespace detail {
template <typename T, typename RTTI, typename Binding, typename Context>
constexpr typename instance_request<RTTI>::derive_function deriver();

// The erased request of T.
template <typename T, typename RTTI, typename Binding, typename Context>
instance_request<RTTI> erased_request() {
  return {RTTI::template get_type_index<request_lookup_type_t<T>>(),
          described_type_v<T>, deriver<T, RTTI, Binding, Context>()};
}

// A reference into the context ends before the caller uses what a resolution
// returns: the caller is refused it, as a static container's compiles.
template <typename Request, typename Scope, typename Context>
void refuse_caller_reference(Scope scope, type_descriptor registered,
                             Context &context) {
  if constexpr (is_fixed_caller_scope_v<Scope>) {
    static_assert(!is_fixed_caller_scope_v<Scope>,
                  "a reference or a pointer to an instance the resolution "
                  "makes would dangle in what is returned to the caller; "
                  "take it by value, or let a shared scope keep what refers "
                  "to it");
  } else if (scope.is_caller()) {
    throw_type_not_convertible(described_type_v<Request>, registered, context);
  }
}

// A wrapper a binding does not publish, emplaced at once of the shallowest of
// the values its layers hold that the binding resolves otherwise than by
// composing it, down the layers that emplace their element: a value is moved
// into all the layers emplaced of it at once, and a handle into the wrapper as
// it is. composes(tag) says whether the binding composes a request, as a
// constant when it is known as the code compiles; with(tag, emplace) passes
// emplace what the binding resolves for a request. Both containers compose a
// request so.
template <typename Wrapper, typename Candidate, typename Composes,
          typename With>
Wrapper compose_wrapper(Composes &composes, With &with) {
  auto emplace = [](auto &&value) -> Wrapper {
    return type_traits<Wrapper>::emplace(std::forward<decltype(value)>(value));
  };
  if constexpr (composes_v<Candidate> && emplaces_element_v<Wrapper> &&
                emplaces_element_v<composed_type_t<Candidate>>) {
    using deeper = composed_inner_t<Candidate>;
    using composed = decltype(composes(request_tag<Candidate>{}));
    if constexpr (std::is_same_v<composed, std::true_type>) {
      return compose_wrapper<Wrapper, deeper>(composes, with);
    } else if constexpr (std::is_same_v<composed, std::false_type>) {
      return with(request_tag<Candidate>{}, emplace);
    } else {
      if (composes(request_tag<Candidate>{})) {
        return compose_wrapper<Wrapper, deeper>(composes, with);
      }
      return with(request_tag<Candidate>{}, emplace);
    }
  } else {
    return with(request_tag<Candidate>{}, emplace);
  }
}

// Derives a request a binding does not publish, of a shape the derivation
// applies to, from the request it is derived from, which inner resolves
// through the binding, and composes says whether the binding composes, as
// compose_wrapper takes it. What the binding keeps is kept, and published to
// the container as it is kept. Both containers derive a request so.
template <derivation How, typename Request, typename Scope, typename Context,
          typename Inner, typename Composes, typename Kept, typename Published>
resolved_address apply_derivation(Scope scope, Context &context,
                                  type_descriptor registered, Inner &&inner,
                                  Composes &composes, Kept &&kept,
                                  Published &&published) {
  using derived = derived_request_t<How, Request>;
  using access = resolved_address::access_kind;
  auto resolve = [&]() -> derived { return inner(request_tag<derived>{}); };
  if constexpr (How == derivation::adopt || How == derivation::take) {
    if constexpr (How == derivation::adopt) {
      refuse_caller_reference<Request>(scope, registered, context);
    }
    derived pointer = resolve();
    place_adopted_pointer(scope, context, pointer);
    return {
        const_cast<std::remove_cv_t<std::remove_pointer_t<derived>> *>(pointer),
        How == derivation::adopt ? access::borrow : access::consume};
  } else if constexpr (How == derivation::place) {
    refuse_caller_reference<Request>(scope, registered, context);
    auto placed = [&]() -> resolved_address {
      auto &value = context.template construct_from<derived>(scope, resolve);
      return {std::addressof(value), access::borrow};
    };
    if constexpr (composes_v<derived>) {
      // A value the binding composes is composed in the context, and referred
      // to there.
      auto referred = [&]() -> resolved_address {
        derived &&value = inner(request_tag<derived &&>{});
        return {std::addressof(value), access::borrow};
      };
      using composed = decltype(composes(request_tag<derived>{}));
      if constexpr (std::is_same_v<composed, std::true_type>) {
        return referred();
      } else if constexpr (std::is_same_v<composed, std::false_type>) {
        return placed();
      } else {
        return composes(request_tag<derived>{}) ? referred() : placed();
      }
    } else {
      return placed();
    }
  } else if constexpr (How == derivation::refer) {
    // The value is resolved first, as a published reference to it is: what
    // fails as it is made fails before the caller is refused it.
    derived value = resolve();
    refuse_caller_reference<Request>(scope, registered, context);
    return {std::addressof(value), access::borrow};
  } else if constexpr (How == derivation::address) {
    derived value = resolve();
    return {const_cast<std::remove_cv_t<std::remove_reference_t<derived>> *>(
                std::addressof(value)),
            access::borrow};
  } else if constexpr (How == derivation::move) {
    derived value = resolve();
    return {std::addressof(value), access::consume};
  } else if constexpr (How == derivation::copy) {
    derived value = resolve();
    return {const_cast<remove_cvref_t<derived> *>(std::addressof(value)),
            access::borrow};
  } else if constexpr (How == derivation::convert) {
    using shared = remove_cvref_t<Request>;
    auto &value = context.template construct_from<shared>(
        scope, [&]() -> shared { return shared(resolve()); });
    return {std::addressof(value), access::consume};
  } else {
    static_assert(How == derivation::keep);
    auto &value =
        kept().template keep<derived>(cache::key<derived>(), context, resolve);
    published(std::addressof(value));
    return {std::addressof(value), access::borrow};
  }
}

// Whether a runtime binding publishes a request, as its site tells.
struct site_publishes {
  const derivation_site &site;

  template <typename T> bool operator()(request_tag<T>) const {
    return site.publishes(described_type_v<T>);
  }
};

template <derivation How, typename T, typename RTTI, typename Binding,
          typename Context>
resolved_address derive_as(const derivation_site &site,
                           construction_scope scope, Binding &binding,
                           Context &context) {
  // Whether the binding composes a request.
  [[maybe_unused]] auto composes = [&](auto tag) -> bool {
    using candidate = typename decltype(tag)::type;
    const type_descriptor requested = described_type_v<candidate>;
    return !site.publishes(requested) &&
           derive_request<candidate>(site.facts_of(requested),
                                     site_publishes{site}) ==
               derivation::compose;
  };
  if constexpr (How == derivation::compose && derivation_shape_v<How, T>) {
    // The wrapper is placed in the context, where the binding hands it over.
    using wrapper = composed_type_t<T>;
    auto with = [&](auto tag, auto &emplace) -> wrapper {
      return emplace(
          resolve_binding_request<typename decltype(tag)::type, RTTI>(
              scope, binding, context));
    };
    auto &value =
        context.template construct_from<wrapper>(scope, [&]() -> wrapper {
          return compose_wrapper<wrapper, derived_request_t<How, T>>(composes,
                                                                     with);
        });
    return {std::addressof(value), resolved_address::access_kind::consume};
  } else if constexpr (derivation_shape_v<How, T>) {
    return apply_derivation<How, T>(
        scope, context, site.registered,
        [&](auto tag) -> typename decltype(tag)::type {
          return resolve_binding_request<typename decltype(tag)::type, RTTI>(
              scope, binding, context);
        },
        composes, [&]() -> retention & { return *site.kept; },
        [&](void *address) {
          if (site.cache_key != nullptr) {
            binding.publish_cache(context, site.cache_key, address);
          }
        });
  } else {
    throw_type_not_convertible(described_type_v<T>, site.registered, context);
  }
}

// Derives a request the binding does not publish from one it resolves.
template <typename T, typename RTTI, typename Binding, typename Context>
resolved_address
derive_binding_request(void *erased, const derivation_site &site,
                       construction_scope scope, void *erased_context) {
  auto &binding = *static_cast<Binding *>(erased);
  auto &context = *static_cast<Context *>(erased_context);
  switch (derive_request<T>(site.facts_of(described_type_v<T>),
                            site_publishes{site})) {
  case derivation::adopt:
    return derive_as<derivation::adopt, T, RTTI>(site, scope, binding, context);
  case derivation::keep:
    return derive_as<derivation::keep, T, RTTI>(site, scope, binding, context);
  case derivation::place:
    return derive_as<derivation::place, T, RTTI>(site, scope, binding, context);
  case derivation::take:
    return derive_as<derivation::take, T, RTTI>(site, scope, binding, context);
  case derivation::refer:
    return derive_as<derivation::refer, T, RTTI>(site, scope, binding, context);
  case derivation::address:
    return derive_as<derivation::address, T, RTTI>(site, scope, binding,
                                                   context);
  case derivation::move:
    return derive_as<derivation::move, T, RTTI>(site, scope, binding, context);
  case derivation::copy:
    return derive_as<derivation::copy, T, RTTI>(site, scope, binding, context);
  case derivation::convert:
    return derive_as<derivation::convert, T, RTTI>(site, scope, binding,
                                                   context);
  case derivation::compose:
    return derive_as<derivation::compose, T, RTTI>(site, scope, binding,
                                                   context);
  case derivation::none:
    break;
  }
  throw_type_not_convertible(described_type_v<T>, site.registered, context);
}

template <typename T, typename RTTI, typename Binding, typename Context>
constexpr typename instance_request<RTTI>::derive_function deriver() {
  if constexpr (derivable_v<T>) {
    return &derive_binding_request<T, RTTI, Binding, Context>;
  } else {
    return nullptr;
  }
}
} // namespace detail

// Resolves a request through a binding. A request the binding does not
// publish is derived from one it resolves.
template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, const void *cache_key) {
  return detail::convert_resolved_binding<T>(binding.resolve_request(
      scope, context, detail::erased_request<T, RTTI, Binding, Context>(),
      cache_key));
}

// A wrapper made of the normalized value is a new object, so a reference
// request is not made of it.
template <typename Request>
inline constexpr bool can_construct_from_normalized_request_v =
    !std::is_reference_v<Request> &&
    type_traits<std::decay_t<Request>>::enabled &&
    !std::is_pointer_v<std::decay_t<Request>> &&
    !std::is_same_v<normalized_type_t<Request>, std::decay_t<Request>>;

// Whether the normalized value of a request is an object that can be built.
// An rvalue request is not one: it asks the routed storage to publish a
// movable result, and is rejected rather than degraded to the normalized value.
// Nor is a bounded array: a pointer to a new one cannot be made with new, and
// a handle of one only decays it.
//
template <typename Request>
inline constexpr bool constructible_normalized_v =
    !std::is_rvalue_reference_v<Request> &&
    std::is_object_v<request_constructed_t<Request>> &&
    !std::is_abstract_v<request_constructed_t<Request>> &&
    std::extent_v<request_constructed_t<Request>> == 0;
// A wrapper is made of the normalized value the request resolves, which it
// copies or moves, by the conversion from that value.
template <typename Request>
inline constexpr bool construct_normalized_request_v = [] {
  if constexpr (can_construct_from_normalized_request_v<Request> &&
                constructible_normalized_v<Request>) {
    using value = request_constructed_t<Request>;
    return (std::is_move_constructible_v<value> ||
            std::is_copy_constructible_v<value>) &&
           detail::is_type_conversion_available_v<std::decay_t<Request>,
                                                  value &&, consume>;
  } else {
    return false;
  }
}();
// Whether the factory constructs an unbound request. A handle of a dynamic
// array is not constructed: a default factory has no size to make it with.
template <typename Request,
          typename ResolvedRequest = request_result_t<Request, true>>
inline constexpr bool construct_factory_request_v =
    constructible_normalized_v<Request> &&
    !std::is_lvalue_reference_v<ResolvedRequest> &&
    !detail::is_array_like_type_v<std::decay_t<ResolvedRequest>> &&
    (std::is_pointer_v<ResolvedRequest> ||
     (type_traits<ResolvedRequest>::enabled &&
      !std::is_reference_v<ResolvedRequest>) ||
     std::is_constructible_v<ResolvedRequest, normalized_type_t<Request>>);

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
