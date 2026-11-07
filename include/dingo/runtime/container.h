//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/complete_type.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/binding/requests.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/registry.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/request.h>
#include <dingo/type/shape.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

// The resolve front-end of a runtime container. It is the root runtime
// registry and adds what a root answers beyond its bindings: the parent
// fallback, auto-construction, and the transaction and cache of a request.
template <typename ContainerTraits, typename Allocator,
          typename ParentContainer>
class detail::runtime_container_impl
    : public runtime_registry<ContainerTraits, Allocator, void,
                              detail::runtime_container_impl<
                                  ContainerTraits, Allocator, ParentContainer>>,
      public detail::container_requests<detail::runtime_container_impl<
          ContainerTraits, Allocator, ParentContainer>> {
  template <typename> friend class ::dingo::resolution_context;

  using self_type = detail::runtime_container_impl<ContainerTraits, Allocator,
                                                   ParentContainer>;
  using runtime_base =
      runtime_registry<ContainerTraits, Allocator, void, self_type>;
  using runtime_context_type =
      runtime_context<typename runtime_base::allocator_type>;
  friend runtime_base;
  template <typename, typename, typename, typename>
  friend class ::dingo::runtime_registry;

public:
  // A container without an explicit parent type accepts a parent of its own
  // type.
  using parent_container_type =
      std::conditional_t<std::is_void_v<ParentContainer>, self_type,
                         detail::container_implementation_t<ParentContainer>>;
  using runtime_registry_type = runtime_base;
  using container_type = self_type;
  using container_traits_type = typename runtime_base::container_traits_type;
  using allocator_type = typename runtime_base::allocator_type;
  using rtti_type = typename runtime_base::rtti_type;
  using lookup_definition_type = typename runtime_base::lookup_definition_type;

private:
  // The parent answers only the lookups it declares.
  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves() {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return detail::has_lookup<parent_container_type, Lookup, LookupKey>();
  }

  using runtime_base::construct_collection;

public:
  runtime_container_impl() : runtime_base(this) {}

  explicit runtime_container_impl(allocator_type alloc)
      : runtime_base(this, alloc) {}

  runtime_container_impl(parent_container_type *parent,
                         const allocator_type &alloc = allocator_type())
      : runtime_base(this, alloc), parent_(parent) {}

  // Resolved instances reference each other by address and runtime bindings
  // point back at the container, so it cannot move. runtime_container moves by
  // owning it through a handle.
  runtime_container_impl(const self_type &) = delete;
  runtime_container_impl(self_type &&) = delete;
  self_type &operator=(const self_type &) = delete;
  self_type &operator=(self_type &&) = delete;

private:
  template <typename Request, typename R> static constexpr bool is_cacheable() {
    return detail::cache::supports_v<typename Request::interface_type> &&
           !collection_traits<R>::is_collection;
  }

  // A request that may be cached reads the cache of its selected binding
  // before it enters a transaction.
  template <typename Request, typename R, typename LookupKey>
  R resolve_entry(LookupKey key) {
    using interface_type = typename Request::interface_type;
    if constexpr (is_cacheable<Request, R>()) {
      auto selected = this->template select_request<Request>(key);
      auto result = this->template lookup_cache<interface_type>(selected);
      if (result.hit) {
        return this->template resolve_cached<interface_type, R>(result);
      }
      return with_context([&](runtime_context_type &context) -> R {
        return detail::request_resolution::resolve<Request, R>(
            *this, selected, caller_scope, context, *this, key);
      });
    } else {
      return with_context([&](runtime_context_type &context) -> R {
        return detail::request_resolution::resolve<Request, R>(
            *this, caller_scope, context, *this, std::move(key));
      });
    }
  }

public:
  // The registry base and the registration and resolution interfaces declare
  // some of the same names; these are the ones a container exposes.
  using detail::container_requests<self_type>::construct;
  using detail::container_requests<self_type>::resolve;
  using runtime_base::get_allocator;
  using runtime_base::register_type;
  using runtime_base::register_type_collection;
  using detail::container_requests<self_type>::construct_collection;
  using detail::container_requests<self_type>::count_collection;

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
    return with_context([&](runtime_context_type &context) -> T {
      return construct_collection<T>(caller_scope, context,
                                     std::forward<Fn>(fn), std::move(key));
    });
  }

  template <typename Fn> decltype(auto) with_context(Fn &&fn) {
    return execute_transaction(this->runtime(), std::forward<Fn>(fn));
  }

  template <typename Request, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    using request = request_type<Request>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    auto status = this->template select_request<request>(key).status;
    if constexpr (parent_serves<typename request::lookup_type,
                                decltype(key)>()) {
      if (parent_ && status == detail::binding_status::not_found) {
        return parent_->template binding_status<Request>(key);
      }
    }
    return status;
  }

  using runtime_base::count_collection;

private:
  friend struct detail::request_resolution;
  friend class detail::container_requests<self_type>;

  parent_container_type *parent() { return parent_; }

  // The request itself if its binding publishes it, otherwise its normalized
  // value.
  template <typename Request, typename R, typename Selection>
  R resolve_exact_or_normalized(Selection, construction_scope scope,
                                runtime_context_type &context) {
    try {
      return detail::request_resolution::resolve<Request, R>(
          *this, scope, context, *this, detail::no_lookup_key());
    } catch (const type_not_convertible_exception &) {
      return detail::request_resolution::resolve_normalized<Request, R>(
          *this, scope, context);
    }
  }

  // A request in a context of another runtime opens a transaction of this one.
  template <typename Request, typename R, typename Origin, typename LookupKey>
  DINGO_ALWAYS_INLINE R resolve_request(construction_scope scope,
                                        runtime_context_type &context,
                                        Origin &origin, LookupKey key) {
    if (context.owns(this->runtime())) {
      return detail::request_resolution::resolve<Request, R>(
          *this, scope, context, origin, std::move(key));
    }
    return resolve_nested<Request, R>(scope, context, origin, std::move(key));
  }
  template <typename Request, typename R, typename Origin, typename LookupKey>
  DINGO_NOINLINE R resolve_nested(construction_scope scope,
                                  runtime_context_type &context, Origin &origin,
                                  LookupKey key) {
    return execute_transaction(
        this->runtime(), context,
        [&](runtime_context_type &local_context) -> R {
          return detail::request_resolution::resolve<Request, R>(
              *this, scope, local_context, origin, std::move(key));
        });
  }

public:
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, runtime_context_type &context,
               LookupKey key) {
    return detail::request_resolution::unresolved<Request, MayAutoConstruct, R>(
        *this, *this, scope, context, key);
  }

private:
  runtime_registry_type &binding_store() { return *this; }

  parent_container_type *parent_ = nullptr;
};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
