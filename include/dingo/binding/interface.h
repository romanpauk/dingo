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
#include <cstdint>

namespace dingo {
struct resolved_address {
  void *address;
  enum class access_kind {
    borrow,
    consume,
  } access;
  // What the last step of the derivation of a value makes, which the request
  // makes where it takes the value: nothing, a copy of the object at the
  // address, of its qualifiers, or the value of the type at a depth of the
  // request at the address moved into the layers above it.
  enum class made_kind : std::uint8_t {
    none,
    copy,
    run
  } made = made_kind::none;
  std::uint8_t depth = 0;
  std::uint8_t quals = 0;
};

namespace detail {
class retention;

struct runtime_request;
namespace execution {
struct binding_layout;
struct runtime_conversion;
} // namespace execution
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
// constant data, and the function of its type that resolves a request of
// it.
template <typename RTTI, typename Context> struct runtime_binding_node {
  using interface_type = runtime_binding_interface<RTTI, Context>;
  using request_type = detail::runtime_request;
  using conversion_type = detail::execution::runtime_conversion;

  // Resolves a request of the binding, of its node.
  resolved_address (*dispatch)(const runtime_binding_node &node,
                               interface_type &binding,
                               construction_scope scope, Context &context,
                               const request_type &request,
                               const void *cache_key);
  type_descriptor registered;
  // The key of the interface of the binding, which a request it resolves
  // names.
  const void *interface_key;
  // The layer sources of what the binding stores, which every request of it
  // is derived from; none when its type has C++ properties the semantics
  // do not model.
  const detail::execution::binding_layout *layout;
  // Whether the binding caches the addresses it resolves.
  bool caches;
  // Where the cell of the binding lies in its record, after its header.
  std::size_t cell_offset;
};

template <typename RTTI, typename Context> class runtime_binding_interface {
public:
  using request_type = detail::runtime_request;
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
