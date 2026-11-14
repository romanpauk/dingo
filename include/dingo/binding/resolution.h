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

#include <array>
#include <cassert>
#include <cstddef>
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

} // namespace detail

template <typename T, typename RTTI, typename Binding, typename Context>
T resolve_binding_request(construction_scope scope, Binding &binding,
                          Context &context, const void *cache_key = nullptr);

namespace detail {
template <typename T, typename RTTI, typename Binding, typename Context>
struct request_node_of;

template <typename RTTI, typename Binding, typename Context>
resolved_address derive_erased(void *erased, const derivation_site &site,
                               construction_scope scope, void *erased_context,
                               const request_node &request);

// The erased request of T: its descriptor, and the node a binding that does
// not publish it derives it of.
template <typename T, typename RTTI, typename Binding, typename Context>
instance_request<RTTI> erased_request() {
  if constexpr (derivable_v<T>) {
    return {described_type_v<T>, &derive_erased<RTTI, Binding, Context>,
            &request_node_of<T, RTTI, Binding, Context>::value};
  } else {
    return {described_type_v<T>};
  }
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

// What a runtime binding tells of itself for a request of T, of its
// descriptor, with whether it publishes the sole owner a sharing handle the
// request asks for converts of, which only the request names.
template <typename T>
derivation_facts runtime_facts(const derivation_site &site,
                               type_descriptor requested) {
  return derivation_facts_of<T>(*site.binding, requested);
}

template <derivation How, typename T, typename RTTI, typename Binding,
          typename Context>
resolved_address derive_as(const derivation_site &site,
                           construction_scope scope, Binding &binding,
                           Context &context) {
  static_assert(derivation_shape_v<How, T>);
  // Whether the binding composes a request.
  [[maybe_unused]] auto composes = [&](auto tag) -> bool {
    using candidate = typename decltype(tag)::type;
    const type_descriptor requested = described_type_v<candidate>;
    return !publishes(*site.binding, requested) &&
           derive_request<candidate>(runtime_facts<candidate>(
               site, requested)) == derivation::compose;
  };
  if constexpr (How == derivation::compose) {
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
  } else {
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
  }
}

// Derives a request of a wrapper: one a binding keeps with it, converts of
// the sole owner it makes, or composes of what it holds. These derivations
// are made of the type of the request, which wrapper_request_t gives.
template <typename T, typename RTTI, typename Binding, typename Context>
resolved_address derive_wrapper(void *erased, const derivation_site &site,
                                construction_scope scope, void *erased_context,
                                derivation how) {
  auto &binding = *static_cast<Binding *>(erased);
  auto &context = *static_cast<Context *>(erased_context);
  if constexpr (derivation_shape_v<derivation::keep, T>) {
    if (how == derivation::keep) {
      return derive_as<derivation::keep, T, RTTI>(site, scope, binding,
                                                  context);
    }
  }
  if constexpr (derivation_shape_v<derivation::convert, T>) {
    if (how == derivation::convert) {
      return derive_as<derivation::convert, T, RTTI>(site, scope, binding,
                                                     context);
    }
  }
  if constexpr (derivation_shape_v<derivation::compose, T>) {
    if (how == derivation::compose) {
      return derive_as<derivation::compose, T, RTTI>(site, scope, binding,
                                                     context);
    }
  }
  throw_type_not_convertible(site.requested, site.registered, context);
}

// The request whose wrapper derivations a request of T shares, so that the
// requests of one wrapper share their code: a reference or a pointer keeps
// the wrapper, as each of them does; a value converts or composes it, as a
// value of any qualification does, and as an rvalue reference does unless it
// composes of another request than the value does.
template <typename T> constexpr auto wrapper_request() {
  using wrapper = composed_type_t<T>;
  if constexpr (refers_request_v<T>) {
    return request_tag<wrapper &>{};
  } else if constexpr (!derivation_shape_v<derivation::compose, T> ||
                       std::is_same_v<composed_inner_t<T>,
                                      composed_inner_t<wrapper>>) {
    return request_tag<wrapper>{};
  } else {
    return request_tag<wrapper &&>{};
  }
}

template <typename T>
using wrapper_request_t = typename decltype(wrapper_request<T>())::type;

// The operations of the object a request is or refers to, erased of its
// type: its size and its alignment; how a value placed for the request is
// copied or moved into memory, as a value is returned of what a binding
// resolves, and destroyed; and how what a pointer the binding hands over
// points to is released. One the request does not need, or the type does not
// have, is null.
struct object_operations {
  using copy_function = void (*)(void *to, void *from);

  std::size_t size;
  std::size_t alignment;
  copy_function copy;
  copy_function move;
  void (*destroy)(void *instance) noexcept;
  // Releases what the pointer kept at owner points to.
  void (*release)(void *owner) noexcept;
};

template <typename T> T copied_object(T &from) { return from; }
template <typename T> T moved_object(T &from) { return std::move(from); }

template <typename T> void copy_object(void *to, void *from) {
  new (to) T(copied_object(*static_cast<T *>(from)));
}

template <typename T> void move_object(void *to, void *from) {
  new (to) T(moved_object(*static_cast<T *>(from)));
}

template <typename T> void destroy_object(void *instance) noexcept {
  static_cast<T *>(instance)->~T();
}

// Deletes the pointer handed over to a request, as the owner a reference
// request adopts it into would: an array as an array.
template <typename T> void release_object(void *owner) noexcept {
  T *pointer = static_cast<T *>(*static_cast<void **>(owner));
  if constexpr (std::is_array_v<T>) {
    std::default_delete<std::remove_extent_t<T>[]>()(*pointer);
  } else {
    std::default_delete<T>()(pointer);
  }
}

// The operations of T a request needs: Places where a value of T may be
// placed for it, Owns where a pointer to T may be handed over to it.
template <typename T, bool Places, bool Owns>
constexpr object_operations make_object_operations() {
  object_operations operations{sizeof(T), alignof(T), nullptr,
                               nullptr,   nullptr,    nullptr};
  if constexpr (Places) {
    if constexpr (is_copy_constructible_v<T>) {
      operations.copy = &copy_object<T>;
    }
    if constexpr (std::is_move_constructible_v<T>) {
      operations.move = &move_object<T>;
    }
    if constexpr (!std::is_trivially_destructible_v<T>) {
      operations.destroy = &destroy_object<T>;
    }
  }
  if constexpr (Owns) {
    operations.release = &release_object<T>;
  }
  return operations;
}

template <typename T, bool Places, bool Owns>
inline constexpr object_operations object_operations_v =
    make_object_operations<T, Places, Owns>();

inline constexpr std::size_t derivation_count =
    static_cast<std::size_t>(derivation::compose) + 1;

// A request a runtime binding derives, erased of its type, so that the
// requests of a shape share the code that derives them: its descriptor, what
// it is, whether a binding derives it at all, the sole owner a sharing handle
// it asks for converts of, the object it is or refers to, and, for each
// derivation of a value, a reference or a pointer, the request it resolves
// through. A value placed for it that a binding composes is referred to as an
// rvalue reference to it. A derivation of a wrapper is made of the type of
// the request, by wrap.
struct request_node {
  using wrap_function = resolved_address (*)(void *binding,
                                             const derivation_site &site,
                                             construction_scope scope,
                                             void *context, derivation how);

  type_descriptor requested;
  request_facts facts;
  bool derivable;
  type_descriptor owner;
  const object_operations *object;
  std::array<const request_node *, derivation_count> through;
  const request_node *referred;
  wrap_function wrap;
};

template <typename T, typename RTTI, typename Binding, typename Context>
struct request_node_of {
  static const request_node value;
};

// Whether a derivation is of a wrapper, which is made of the type of the
// request.
constexpr bool derives_wrapper(derivation how) {
  return how == derivation::keep || how == derivation::convert ||
         how == derivation::compose;
}

template <derivation How, typename T, typename RTTI, typename Binding,
          typename Context>
constexpr const request_node *through_node() {
  if constexpr (derivation_shape_v<How, T> && !derives_wrapper(How)) {
    return &request_node_of<derived_request_t<How, T>, RTTI, Binding,
                            Context>::value;
  } else {
    return nullptr;
  }
}

template <typename T, typename RTTI, typename Binding, typename Context>
constexpr std::array<const request_node *, derivation_count> through_nodes() {
  return {through_node<derivation::none, T, RTTI, Binding, Context>(),
          through_node<derivation::adopt, T, RTTI, Binding, Context>(),
          through_node<derivation::keep, T, RTTI, Binding, Context>(),
          through_node<derivation::refer, T, RTTI, Binding, Context>(),
          through_node<derivation::address, T, RTTI, Binding, Context>(),
          through_node<derivation::place, T, RTTI, Binding, Context>(),
          through_node<derivation::take, T, RTTI, Binding, Context>(),
          through_node<derivation::move, T, RTTI, Binding, Context>(),
          through_node<derivation::copy, T, RTTI, Binding, Context>(),
          through_node<derivation::convert, T, RTTI, Binding, Context>(),
          through_node<derivation::compose, T, RTTI, Binding, Context>()};
}

template <typename T> constexpr const object_operations *object_of() {
  constexpr bool places = derivation_shape_v<derivation::place, T>;
  constexpr bool owns = derivation_shape_v<derivation::adopt, T> ||
                        derivation_shape_v<derivation::take, T>;
  if constexpr (places || owns) {
    return &object_operations_v<referred_value_t<T>, places, owns>;
  } else {
    return nullptr;
  }
}

template <typename T, typename RTTI, typename Binding, typename Context>
constexpr const request_node *referred_node() {
  if constexpr (derivation_shape_v<derivation::place, T>) {
    if constexpr (composes_v<referred_value_t<T>>) {
      return &request_node_of<referred_value_t<T> &&, RTTI, Binding,
                              Context>::value;
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

template <typename T, typename RTTI, typename Binding, typename Context>
constexpr request_node::wrap_function wrapper_of() {
  if constexpr (derivation_shape_v<derivation::keep, T> ||
                derivation_shape_v<derivation::convert, T> ||
                derivation_shape_v<derivation::compose, T>) {
    return &derive_wrapper<wrapper_request_t<T>, RTTI, Binding, Context>;
  } else {
    return nullptr;
  }
}

template <typename T, typename RTTI, typename Binding, typename Context>
const request_node request_node_of<T, RTTI, Binding, Context>::value{
    described_type_v<T>,
    request_facts_v<T>,
    derivable_v<T>,
    converted_owner<T>(),
    object_of<T>(),
    through_nodes<T, RTTI, Binding, Context>(),
    referred_node<T, RTTI, Binding, Context>(),
    wrapper_of<T, RTTI, Binding, Context>()};

// What a runtime binding tells of itself for a request, of the descriptor it
// was dispatched with, with whether it publishes the sole owner a sharing
// handle the request asks for converts of, which only the request names.
inline derivation_facts node_facts(const derivation_site &site,
                                   const request_node &request,
                                   type_descriptor requested) {
  return derivation_facts_of(*site.binding, requested,
                             request.facts.shapes.has(derivation::convert),
                             request.owner);
}

// Resolves the request of a node through a binding.
template <typename RTTI, typename Binding, typename Context>
resolved_address resolve_node(construction_scope scope, Binding &binding,
                              Context &context, const request_node &request) {
  return binding.resolve_request(
      scope, context,
      instance_request<RTTI>{
          request.requested,
          request.derivable ? &derive_erased<RTTI, Binding, Context> : nullptr,
          &request},
      nullptr);
}

// Places a pointer a binding hands over in the context, which releases what
// it points to as it ends.
template <typename Context>
void own_pointer(construction_scope scope, Context &context,
                 const object_operations &object, void *pointer) {
  bool owned = false;
  try {
    context.construct_erased(scope, sizeof(void *), alignof(void *),
                             object.release, [&](void *owner) {
                               new (owner) void *(pointer);
                               owned = true;
                             });
  } catch (...) {
    if (!owned) {
      object.release(&pointer);
    }
    throw;
  }
}

// Places a value made of what a binding resolves in the context: moved out of
// what is handed over, copied of anything else, as a value is returned of
// what the binding resolves.
template <typename Context>
void *place_value(construction_scope scope, Context &context,
                  const request_node &value, const object_operations &object,
                  resolved_address resolved) {
  object_operations::copy_function make = nullptr;
  if (resolved.access == resolved_address::access_kind::consume &&
      object.move != nullptr) {
    make = object.move;
  } else if (object.copy != nullptr) {
    make = object.copy;
  } else if (object.move != nullptr) {
    assert(resolved.access == resolved_address::access_kind::consume);
    make = object.move;
  } else {
    throw_type_not_convertible(value.requested, value.requested);
  }
  return context.construct_erased(
      scope, object.size, object.alignment, object.destroy,
      [&](void *at) { make(at, resolved.address); });
}

// Derives a request the binding does not publish, of its node, from one it
// resolves. A value, a reference or a pointer is derived here, of the
// descriptors of the requests and the operations of their object, so that
// the code is shared by every request; a wrapper by its type.
template <typename RTTI, typename Binding, typename Context>
resolved_address derive_erased(void *erased, const derivation_site &site,
                               construction_scope scope, void *erased_context,
                               const request_node &request) {
  using access = resolved_address::access_kind;
  auto &binding = *static_cast<Binding *>(erased);
  auto &context = *static_cast<Context *>(erased_context);
  auto through = [&](derivation how) -> resolved_address {
    return resolve_node<RTTI>(scope, binding, context,
                              *request.through[static_cast<std::size_t>(how)]);
  };
  // A reference into the context ends before the caller uses what a
  // resolution returns: the caller is refused it.
  auto refuse_caller = [&] {
    if (scope.is_caller()) {
      throw_type_not_convertible(request.requested, site.registered, context);
    }
  };
  // The request is derived of the descriptor it was dispatched with.
  const derivation how =
      derive_request(request.facts, node_facts(site, request, site.requested));
  switch (how) {
  case derivation::adopt: {
    refuse_caller();
    void *pointer = through(how).address;
    own_pointer(scope, context, *request.object, pointer);
    return {pointer, access::borrow};
  }
  case derivation::take: {
    void *pointer = through(how).address;
    own_pointer(scope, context, *request.object, pointer);
    return {pointer, access::consume};
  }
  case derivation::refer: {
    // The value is resolved first, as a published reference to it is: what
    // fails as it is made fails before the caller is refused it.
    void *address = through(how).address;
    refuse_caller();
    return {address, access::borrow};
  }
  case derivation::address:
  case derivation::copy:
    return {through(how).address, access::borrow};
  case derivation::move:
    return {through(how).address, access::consume};
  case derivation::place: {
    refuse_caller();
    const request_node &value = *request.through[static_cast<std::size_t>(how)];
    // A value the binding composes is composed in the context, and referred
    // to there.
    if (request.referred != nullptr &&
        !publishes(*site.binding, value.requested) &&
        derive_request(value.facts, node_facts(site, value, value.requested)) ==
            derivation::compose) {
      return {resolve_node<RTTI>(scope, binding, context, *request.referred)
                  .address,
              access::borrow};
    }
    return {place_value(scope, context, value, *request.object,
                        resolve_node<RTTI>(scope, binding, context, value)),
            access::borrow};
  }
  case derivation::keep:
  case derivation::convert:
  case derivation::compose:
    return request.wrap(erased, site, scope, erased_context, how);
  case derivation::none:
    break;
  }
  throw_type_not_convertible(request.requested, site.registered, context);
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
