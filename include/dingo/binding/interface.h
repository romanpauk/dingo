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
#include <dingo/binding/materialization.h>
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

// What the bindings of the interfaces of one registration share: the request
// cache of their interfaces, the container the registration is made in, and
// the level of its local bindings once there is one. The cell of the storage
// of the registration follows it.
struct runtime_binding_header {
  detail::cache::entry cache;
  void *container;
  void *level;
};

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
  // How the source of the binding is materialized, which the static
  // container decides by too.
  detail::materialization_facts materialization;
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

  // The binding of an interface of a registration, of its node, in the
  // record of the registration.
  runtime_binding_interface(const node_type *node,
                            runtime_binding_header *header) noexcept
      : node_(node), header_(header) {}

  runtime_binding_header &header() noexcept { return *header_; }

  detail::cache::entry *cache_slot() noexcept { return &header_->cache; }

  // A null cache_key means the request is not cached.
  resolved_address resolve_request(construction_scope scope, Context &context,
                                   const request_type &request,
                                   const void *cache_key) {
    return node_->dispatch(*node_, *this, scope, context, request, cache_key);
  }

  // Publishes a stable address for the request key until the transaction
  // that resolved it is rolled back.
  void publish_cache(Context &context, const void *key, void *address) {
    auto *slot = cache_slot();
    context.defer_rollback(
        +[](void *published, void *) noexcept {
          *static_cast<detail::cache::entry *>(published) = {};
        },
        slot);
    *slot = {key, address};
  }

private:
  const node_type *node_;
  runtime_binding_header *header_;
};
} // namespace dingo
