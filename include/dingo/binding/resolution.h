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

template <typename T> T resolved_value(resolved_address result) {
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
decltype(auto) consume_resolved(Instance &&instance, Fn &&fn) {
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
T resolve_binding(construction_scope scope, Binding &binding, Context &context,
                  const void *cache_key = nullptr);

namespace detail {
template <typename T, typename RTTI, typename Binding, typename Context>
constexpr const request_node *request_node_v();

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
            request_node_v<T, RTTI, Binding, Context>()};
  } else {
    return {described_type_v<T>};
  }
}

// A reference into the context ends before the caller uses what a resolution
// returns: the caller is refused it, as a static container's compiles.
template <typename Request, typename Scope, typename Context>
void reject_escape(Scope scope, type_descriptor registered, Context &context) {
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
      reject_escape<Request>(scope, registered, context);
    }
    derived pointer = resolve();
    own_pointer(scope, context, pointer);
    return {
        const_cast<std::remove_cv_t<std::remove_pointer_t<derived>> *>(pointer),
        How == derivation::adopt ? access::borrow : access::consume};
  } else if constexpr (How == derivation::place) {
    reject_escape<Request>(scope, registered, context);
    auto placed = [&]() -> resolved_address {
      auto &value = context.template construct_made<derived>(scope, resolve);
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
    reject_escape<Request>(scope, registered, context);
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
    auto &value = context.template construct_made<shared>(
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
  return binding_derivations<T>(*site.binding, requested);
}

template <derivation How, typename T, typename RTTI, typename Binding,
          typename Context>
resolved_address derive(const derivation_site &site, construction_scope scope,
                        Binding &binding, Context &context) {
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
      return emplace(resolve_binding<typename decltype(tag)::type, RTTI>(
          scope, binding, context));
    };
    auto &value =
        context.template construct_made<wrapper>(scope, [&]() -> wrapper {
          return compose_wrapper<wrapper, derived_request_t<How, T>>(composes,
                                                                     with);
        });
    return {std::addressof(value), resolved_address::access_kind::consume};
  } else {
    return apply_derivation<How, T>(
        scope, context, site.registered,
        [&](auto tag) -> typename decltype(tag)::type {
          return resolve_binding<typename decltype(tag)::type, RTTI>(
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
// are made of the type of the wrapper, as make_table_parts gives it.
template <typename T, typename RTTI, typename Binding, typename Context>
resolved_address derive_wrapper(void *erased, const derivation_site &site,
                                construction_scope scope, void *erased_context,
                                derivation how) {
  auto &binding = *static_cast<Binding *>(erased);
  auto &context = *static_cast<Context *>(erased_context);
  if constexpr (derivation_shape_v<derivation::keep, T>) {
    if (how == derivation::keep) {
      return derive<derivation::keep, T, RTTI>(site, scope, binding, context);
    }
  }
  if constexpr (derivation_shape_v<derivation::convert, T>) {
    if (how == derivation::convert) {
      return derive<derivation::convert, T, RTTI>(site, scope, binding,
                                                  context);
    }
  }
  if constexpr (derivation_shape_v<derivation::compose, T>) {
    if (how == derivation::compose) {
      return derive<derivation::compose, T, RTTI>(site, scope, binding,
                                                  context);
    }
  }
  throw_type_not_convertible(site.requested, site.registered, context);
}

// The request whose wrapper derivations an rvalue reference to a wrapper
// shares: the value of it, unless it composes of another request than the
// value does. A reference or a pointer to the wrapper shares those of an
// lvalue reference to it, and a value of any qualification those of the
// value.
template <typename Wrapper> constexpr auto rvalue_wrapper_request() {
  if constexpr (derivation_shape_v<derivation::compose, Wrapper> &&
                !std::is_same_v<composed_inner_t<Wrapper &&>,
                                composed_inner_t<Wrapper>>) {
    return request_tag<Wrapper &&>{};
  } else {
    return request_tag<Wrapper>{};
  }
}

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

// The operations of T the requests of it need: Places where a value of T may
// be placed for one; a pointer to T may be handed over to one.
template <typename T, bool Places>
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
  operations.release = &release_object<T>;
  return operations;
}

template <typename T, bool Places>
inline constexpr object_operations object_operations_v =
    make_object_operations<T, Places>();

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

// What the nodes of the requests of an object are made of, of its type: the
// descriptors of the object as the table qualifies it, as is and const; the
// sole owner a sharing handle of it converts of; its operations; the
// derivations of it as a wrapper, kept for a reference or a pointer,
// converted or composed for a value and for an rvalue reference; the table of
// the object as not volatile, which a derivation other than adopting a
// pointer or taking an address resolves through; and the node of a value of
// it, which is a pointer of the table of what it points to when it is a
// pointer.
struct table_parts {
  const type_descriptor *object;
  const type_descriptor *const_object;
  type_descriptor owner;
  const object_operations *operations;
  request_node::wrap_function keep;
  request_node::wrap_function value;
  request_node::wrap_function rvalue;
  const request_node *plain;
  const request_node *placed;
};

constexpr type_descriptor referenced(type_descriptor descriptor,
                                     type_reference_kind reference) {
  descriptor.reference = reference;
  return descriptor;
}

constexpr type_descriptor shape_descriptor(const table_parts &parts,
                                           request_shape shape) {
  const bool is_const = static_cast<unsigned>(shape) % 2 != 0;
  const type_descriptor *object = is_const ? parts.const_object : parts.object;
  switch (shape) {
  case request_shape::lvalue:
  case request_shape::const_lvalue:
    return referenced(*object, type_reference_kind::lvalue);
  case request_shape::rvalue:
  case request_shape::const_rvalue:
    return referenced(*object, type_reference_kind::rvalue);
  case request_shape::pointer:
  case request_shape::const_pointer:
    return {
        {}, nullptr, type_cv_flags::none, type_reference_kind::none, object};
  default:
    return *object;
  }
}

constexpr const request_node *shape_row(const request_node *table,
                                        request_shape shape) {
  return table + static_cast<std::size_t>(shape);
}

// The node of a request of a shape of an object, in the table of its
// qualification. A request that places a value or owns a pointer has the
// operations of the object.
constexpr request_node shape_node(object_traits object,
                                  const table_parts &parts, request_shape shape,
                                  const request_node *table) {
  const request_facts facts = shape_facts(object, shape);
  const flags<derivation> shapes = facts.shapes;
  const bool is_const = static_cast<unsigned>(shape) % 2 != 0;
  request_node node{shape_descriptor(parts, shape),
                    facts,
                    derivable(facts),
                    shapes.has(derivation::convert) ? parts.owner
                                                    : type_descriptor{},
                    nullptr,
                    {},
                    nullptr,
                    nullptr};
  if (shapes.has(derivation::place) || shapes.has(derivation::adopt) ||
      shapes.has(derivation::take)) {
    node.object = parts.operations;
  }
  const request_node *through[derivation_count] = {
      nullptr,
      shape_row(table, is_const ? request_shape::const_pointer
                                : request_shape::pointer),
      nullptr,
      shape_row(parts.plain, request_shape::rvalue),
      shape_row(table,
                is_const ? request_shape::const_lvalue : request_shape::lvalue),
      parts.placed,
      shape_row(parts.plain, request_shape::pointer),
      shape_row(parts.plain, request_shape::rvalue),
      shape_row(parts.plain, request_shape::const_lvalue),
      nullptr,
      nullptr};
  for (std::size_t how = 0; how < derivation_count; ++how) {
    if (shapes.has(static_cast<derivation>(how))) {
      node.through[how] = through[how];
    }
  }
  if (shapes.has(derivation::place) && object.composes) {
    node.referred = shape_row(parts.plain, request_shape::rvalue);
  }
  if (shapes.has(derivation::keep)) {
    node.wrap = parts.keep;
  } else if (shapes.has(derivation::convert) ||
             shapes.has(derivation::compose)) {
    node.wrap = value_shape(shape) ? parts.value : parts.rvalue;
  }
  return node;
}

// The nodes of the requests of the shapes of an object, as volatile or not.
template <typename Object, bool Volatile, typename RTTI, typename Binding,
          typename Context>
struct request_table {
  static const request_node nodes[request_shape_count];
};

template <typename Object, bool Volatile, typename RTTI, typename Binding,
          typename Context>
constexpr table_parts make_table_parts() {
  using qualified = std::conditional_t<Volatile, volatile Object, Object>;
  constexpr object_traits traits = object_traits_v<Object>;
  table_parts parts{&type_descriptor_v<qualified>,
                    &type_descriptor_v<const qualified>,
                    {},
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                    request_table<Object, false, RTTI, Binding, Context>::nodes,
                    nullptr};
  if constexpr (traits.converts) {
    parts.owner = described_type_v<typename converted_owner<Object>::type>;
  }
  if constexpr (traits.complete) {
    parts.operations = &object_operations_v<Object, traits.places>;
  }
  if constexpr (traits.places && std::is_pointer_v<Object>) {
    parts.placed = request_node_v<Object, RTTI, Binding, Context>();
  } else if constexpr (traits.places) {
    parts.placed = shape_row(parts.plain, request_shape::value);
  }
  if constexpr (traits.shares) {
    parts.keep = &derive_wrapper<Object &, RTTI, Binding, Context>;
  }
  if constexpr (traits.converts || traits.composes) {
    parts.value = &derive_wrapper<Object, RTTI, Binding, Context>;
    parts.rvalue = &derive_wrapper<
        typename decltype(rvalue_wrapper_request<Object>())::type, RTTI,
        Binding, Context>;
  }
  return parts;
}

template <typename Object, bool Volatile, typename RTTI, typename Binding,
          typename Context>
inline constexpr table_parts table_parts_v =
    make_table_parts<Object, Volatile, RTTI, Binding, Context>();

template <typename Object, bool Volatile, typename RTTI, typename Binding,
          typename Context>
const request_node request_table<Object, Volatile, RTTI, Binding,
                                 Context>::nodes[request_shape_count] = {
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::value, nodes),
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::const_value, nodes),
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::lvalue, nodes),
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::const_lvalue, nodes),
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::rvalue, nodes),
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::const_rvalue, nodes),
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::pointer, nodes),
    shape_node(object_traits_v<Object>,
               table_parts_v<Object, Volatile, RTTI, Binding, Context>,
               request_shape::const_pointer, nodes)};

// The node of a request: of its shape, in the table of its object.
template <typename T, typename RTTI, typename Binding, typename Context>
constexpr const request_node *request_node_v() {
  using object = request_object<T>;
  return request_table<typename object::type, object::is_volatile, RTTI,
                       Binding, Context>::nodes +
         static_cast<std::size_t>(object::shape);
}

// What a runtime binding tells of itself for a request, of the descriptor it
// was dispatched with, with whether it publishes the sole owner a sharing
// handle the request asks for converts of, which only the request names.
inline derivation_facts node_facts(const derivation_site &site,
                                   const request_node &request,
                                   type_descriptor requested) {
  return binding_derivations(*site.binding, requested,
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
T resolve_binding(construction_scope scope, Binding &binding, Context &context,
                  const void *cache_key) {
  return detail::resolved_value<T>(binding.resolve_request(
      scope, context, detail::erased_request<T, RTTI, Binding, Context>(),
      cache_key));
}

// A wrapper made of the normalized value is a new object, so a reference
// request is not made of it.
template <typename Request>
inline constexpr bool constructs_normalized_v =
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
  if constexpr (constructs_normalized_v<Request> &&
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
