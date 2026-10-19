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
#include <dingo/resolution/cache.h>
#include <dingo/type/rebind_type.h>

#include <cassert>

namespace dingo {
template <typename Allocator> class runtime_context;

template <typename RTTI> struct instance_request {
  using type_index = typename RTTI::type_index;

  type_index lookup_type;
  type_descriptor requested_type;
};

template <typename T> struct request_lookup_type {
  using type = rebind_leaf_t<T, runtime_type>;
};

template <typename T>
struct request_lookup_type<const T> : request_lookup_type<T> {};

template <typename T>
struct request_lookup_type<volatile T> : request_lookup_type<T> {};

template <typename T>
struct request_lookup_type<const volatile T> : request_lookup_type<T> {};

template <typename T> struct request_lookup_type<T *> {
  using type = std::add_pointer_t<typename request_lookup_type<T>::type>;
};

template <typename T> struct request_lookup_type<T &> {
  using type =
      std::add_lvalue_reference_t<typename request_lookup_type<T>::type>;
};

template <typename T> struct request_lookup_type<T &&> {
  using type =
      std::add_rvalue_reference_t<typename request_lookup_type<T>::type>;
};

template <typename T>
using request_lookup_type_t = typename request_lookup_type<T>::type;

struct resolved_address {
  void *address;
  enum class access_kind {
    borrow,
    consume,
  } access;
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
