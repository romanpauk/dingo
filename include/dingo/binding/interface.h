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

namespace dingo {
namespace detail {
struct derivation_site;
} // namespace detail

struct resolved_address {
  void *address;
  enum class access_kind {
    borrow,
    consume,
  } access;
};

template <typename RTTI> struct instance_request {
  using type_index = typename RTTI::type_index;
  // Derives a request the binding does not publish from one it resolves
  // through the binding.
  using derive_function =
      resolved_address (*)(void *binding, const detail::derivation_site &site,
                           construction_scope scope, void *context);

  type_index lookup_type;
  type_descriptor requested_type;
  // Called only when the binding does not publish the request.
  derive_function derive = nullptr;
};

template <typename RTTI, typename Context> class runtime_binding_interface {
public:
  using request_type = instance_request<RTTI>;
  using resolve_function = resolved_address (*)(runtime_binding_interface &,
                                                construction_scope, Context &,
                                                const request_type &,
                                                const void *);

  detail::cache::entry *cache_slot() noexcept { return cache_slot_; }

  // A null cache_key means the request is not cached.
  resolved_address resolve_request(construction_scope scope, Context &context,
                                   const request_type &request,
                                   const void *cache_key) {
    return resolve_(*this, scope, context, request, cache_key);
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
  explicit runtime_binding_interface(resolve_function resolve)
      : resolve_(resolve) {}
  ~runtime_binding_interface() = default;

  void cache_slot(detail::cache::entry *slot) noexcept { cache_slot_ = slot; }

private:
  resolve_function resolve_;
  detail::cache::entry *cache_slot_ = nullptr;
};
} // namespace dingo
