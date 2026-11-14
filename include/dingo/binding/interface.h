//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/type_descriptor.h>
#include <dingo/binding/cache.h>
#include <dingo/request/decomposition.h>

#include <cassert>
#include <cstddef>

namespace dingo {
namespace detail {
struct derivation_site;
struct request_node;
} // namespace detail

struct resolved_address {
  void *address;
  enum class access_kind {
    borrow,
    consume,
  } access;
};

template <typename RTTI> struct instance_request {
  // Derives a request the binding does not publish, of its node, from one it
  // resolves through the binding.
  using derive_function =
      resolved_address (*)(void *binding, const detail::derivation_site &site,
                           construction_scope scope, void *context,
                           const detail::request_node &request);

  type_descriptor requested_type;
  // Called only when the binding does not publish the request; none where
  // no binding derives it.
  derive_function derive = nullptr;
  const detail::request_node *node = nullptr;
};

namespace detail {
struct binding_facts;
class retention;
} // namespace detail

template <typename RTTI, typename Context> class runtime_binding_interface;

// A runtime binding as the runtime container resolves it: what it is, as
// constant data, and the functions of its type. Everything else is the engine
// of the container, which dispatches every binding by its node.
template <typename RTTI, typename Context> struct runtime_binding_node {
  using interface_type = runtime_binding_interface<RTTI, Context>;
  using request_type = instance_request<RTTI>;

  // The engine: resolves a request of a binding, of its node. It is one
  // function for every binding of a container.
  resolved_address (*dispatch)(const runtime_binding_node &node,
                               interface_type &binding,
                               construction_scope scope, Context &context,
                               const request_type &request,
                               const void *cache_key);
  // What the binding publishes and tells of itself.
  const detail::binding_facts *facts;
  type_descriptor registered;
  // The leaf of what the binding stores, which its recursion guard names.
  type_descriptor leaf;
  // Whether the binding is guarded against recursion on every resolution,
  // or while its storage is unresolved.
  bool guards_always;
  bool guards_unresolved;
  // Whether an unresolved storage retains the source it materializes.
  bool retains_unresolved;
  // Whether the storage places a value in the context.
  bool places;
  // Whether the binding caches the addresses it resolves.
  bool caches;
  // Materializes the source of the binding, in the construction scope, and
  // applies to it the resolution it publishes at an index, in the scope of
  // the request.
  resolved_address (*resolve)(interface_type &binding, construction_scope scope,
                              construction_scope construction, Context &context,
                              std::size_t index, type_descriptor requested);
  void *(*storage)(interface_type &binding) noexcept;
  void *(*cell)(interface_type &binding) noexcept;
  // What the binding keeps of its instance, or none.
  detail::retention *(*retained)(interface_type &binding) noexcept;
  bool (*resolved)(void *storage) noexcept;
  void (*reset)(void *cell) noexcept;
};

template <typename RTTI, typename Context> class runtime_binding_interface {
public:
  using request_type = instance_request<RTTI>;
  using node_type = runtime_binding_node<RTTI, Context>;

  detail::cache::entry *cache_slot() noexcept { return cache_slot_; }

  // A null cache_key means the request is not cached.
  resolved_address resolve_request(construction_scope scope, Context &context,
                                   const request_type &request,
                                   const void *cache_key) {
    return node_->dispatch(*node_, *this, scope, context, request, cache_key);
  }

  // Publishes a stable address for the request key until the transaction
  // that resolved it is rolled back.
  void publish_cache(Context &context, const void *key, void *address) {
    assert(cache_slot_ != nullptr);
    context.on_rollback(
        +[](void *slot, void *) noexcept {
          *static_cast<detail::cache::entry *>(slot) = {};
        },
        cache_slot_);
    *cache_slot_ = {key, address};
  }

protected:
  // Runtime lookup is non-owning; transaction storage destroys concrete
  // bindings, so dispatch does not need a distinct virtual base per container.
  explicit runtime_binding_interface(const node_type *node) : node_(node) {}
  ~runtime_binding_interface() = default;

  void cache_slot(detail::cache::entry *slot) noexcept { cache_slot_ = slot; }

private:
  const node_type *node_;
  detail::cache::entry *cache_slot_ = nullptr;
};
} // namespace dingo
