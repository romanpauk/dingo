//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/config.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/binding/container_miss.h>
#include <dingo/binding/container_selection.h>
#include <dingo/container_base.h>
#include <dingo/container_collections.h>
#include <dingo/container_operations.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/registration/type_registration.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/registration.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/request_lift.h>

#include <cstdlib>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

// The runtime container: one registry that owns runtime registrations, resolves
// through them, and falls back to its parent container. It has no compile-time
// source; static_container is the other container shape.
template <typename ParentContainer,
          typename ContainerTraits = dynamic_container_traits,
          typename Allocator = typename ContainerTraits::allocator_type>
class runtime_container_impl
    : public detail::runtime_registration<detail::runtime_container_impl<
          ParentContainer, ContainerTraits, Allocator>>,
      public detail::container_collections<detail::runtime_container_impl<
          ParentContainer, ContainerTraits, Allocator>>,
      public detail::container_selection<detail::runtime_container_impl<
          ParentContainer, ContainerTraits, Allocator>>,
      public detail::container_operations<
          detail::runtime_container_impl<ParentContainer, ContainerTraits,
                                         Allocator>,
          true> {
  template <typename> friend class runtime_context;
  template <typename> friend class detail::container_selection;
  template <typename, typename, typename>
  friend class detail::runtime_container_impl;
  template <typename Policy, bool MayAutoConstruct, bool StaticError,
            typename Request, typename LookupKey, typename R,
            typename FriendContainer, typename FriendContext>
  friend R resolve_missing_request(FriendContainer &, construction_scope,
                                   FriendContext &, LookupKey);
  template <typename, bool> friend class container_operations;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;

private:
  using self_type =
      runtime_container_impl<ParentContainer, ContainerTraits, Allocator>;
  using selection_type = detail::container_selection<self_type>;
  using runtime_base =
      runtime_registry<ContainerTraits, Allocator, void, self_type, true>;
  using runtime_context_type = runtime_context<Allocator>;
  friend runtime_base;
  template <typename, typename, typename, typename, bool>
  friend class ::dingo::runtime_registry;

public:
  using parent_container_type =
      std::conditional_t<std::is_void_v<ParentContainer>, self_type,
                         detail::container_implementation_t<ParentContainer>>;
  using runtime_registry_type = runtime_base;
  using registry_type = runtime_base;
  using container_type = self_type;
  using runtime_selection = typename runtime_registry_type::runtime_selection;

private:
  static constexpr bool has_parent_v = !std::is_void_v<parent_container_type>;

  static constexpr bool has_parent = has_parent_v;
  static constexpr bool has_runtime_source = true;

  template <typename T>
  static constexpr bool may_auto_construct_v =
      [] { return detail::is_runtime_auto_constructible_dependency_v<T>; }();

  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves_v =
      has_parent_v &&
      detail::has_lookup<parent_container_type, Lookup, LookupKey>();

public:
  runtime_registry_type &runtime_source() { return runtime_registry_; }
  parent_container_type *parent() { return parent_; }

  template <typename Request, typename R = typename Request::lookup_type>
  R resolve_request(construction_scope scope, runtime_context_type &context) {
    auto key = detail::no_lookup_key();
    auto selected = runtime_registry_.template select_binding<
        typename Request::value_type, typename Request::exact_type>(key);
    return detail::resolve_selection<R, typename Request::lookup_type>(
        selected, context,
        [&]() -> R {
          return runtime_registry_
              .template resolve_named<typename Request::interface_type, R>(
                  selected, scope, context);
        },
        [&]() -> R {
          return this->template unresolved<
              Request,
              detail::is_runtime_auto_constructible_dependency_v<
                  typename Request::user_type>,
              detail::no_lookup_key_t, R>(scope, context, std::move(key));
        });
  }

  template <typename Request, typename R = typename Request::result_type>
  R construct_resolved_request(construction_scope scope,
                               runtime_context_type &context) {
    using user_type = typename Request::user_type;
    using request_value_type = typename Request::value_type;
    return ::dingo::construct_resolved_request<user_type>(
        [&]() -> R { return resolve_request<Request>(scope, context); },
        [&]() {
          return resolve_request<request_type<request_value_type>>(scope,
                                                                   context);
        });
  }

  template <typename Request,
            typename Factory = constructor<typename Request::value_type>,
            typename R = typename Request::result_type>
  R construct_request(construction_scope scope, runtime_context_type &context,
                      Factory factory = Factory()) {
    using user_type = typename Request::user_type;
    using request_value_type = typename Request::value_type;

    if constexpr (std::is_same_v<Factory, constructor<request_value_type>>) {
      auto key = detail::no_lookup_key();
      if (runtime_registry_.template binding_status<
              typename Request::value_type, typename Request::exact_type>(
              key) != detail::binding_status::not_found) {
        if constexpr (!construct_normalized_request_v<user_type> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          user_type>) {
          return selection_type::template resolve<Request, R>(scope, context,
                                                              *this, key);
        } else {
          return construct_resolved_request<Request, R>(scope, context);
        }
      }
    }

    if constexpr (construct_factory_request_v<user_type>) {
      return factory.template construct<R>(scope, context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             user_type>) {
      ::dingo::throw_missing_rvalue_conversion<user_type>(false, context);
    } else {
      auto key = detail::no_lookup_key();
      return selection_type::template resolve<Request, R>(scope, context, *this,
                                                          key);
    }
  }

private:
  template <typename Request, typename R, typename LookupKey>
  static constexpr bool cacheable() {
    if constexpr (!detail::cache::supports_v<
                      typename Request::interface_type> ||
                  collection_traits<R>::is_collection) {
      return false;
    } else {
      return true;
    }
  }

  template <typename Request, typename R>
  DINGO_NOINLINE R
  resolve_selected(typename runtime_registry_type::runtime_selection selected) {
    using interface_type = typename Request::interface_type;
    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
          return runtime_registry_.template resolve_named<interface_type, R>(
              selected, ephemeral_scope, context);
        });
  }

  template <typename Request, typename R, typename LookupKey>
  R resolve_entry_direct(LookupKey key) {
    using interface_type = typename Request::interface_type;
    if constexpr (cacheable<Request, R, LookupKey>()) {
      auto selected = runtime_registry_.template select_binding<
          typename Request::value_type, typename Request::exact_type>(key);
      auto result =
          runtime_registry_.template lookup_cache<interface_type>(selected);
      if (result.hit) {
        return runtime_registry_.template resolve_cached<interface_type, R>(
            result);
      }
      if (selected.status == detail::binding_status::found) {
        return resolve_selected<Request, R>(selected);
      }
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            return selection_type::template resolve_selected<Request, R>(
                selected, ephemeral_scope, context, *this, std::move(key));
          });
    } else {
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            return selection_type::template resolve<Request, R>(
                ephemeral_scope, context, *this, std::move(key));
          });
    }
  }

  // A request for a wrapper type that the binding does not publish is lifted
  // from its value. Direct routes still win; a missing route raises the
  // distinct exception the lift catches.
  template <typename Request, typename R, typename LookupKey>
  R resolve_entry(LookupKey key) {
    using user_type = typename Request::user_type;
    using policy =
        detail::resolution_policy<user_type, LookupKey, static_bindings<>,
                                  detail::resolution_mode::runtime_fallback>;
    if constexpr (!policy::lift_enabled) {
      return resolve_entry_direct<Request, R>(std::move(key));
    }
    return policy::template resolve<R>(
        [&](auto) -> R {
          return resolve_entry_direct<Request, R>(std::move(key));
        },
        [&](auto tag) -> R {
          using selected_policy = typename decltype(tag)::policy;
          return selected_policy::lift_type::make(
              resolve<typename selected_policy::inner_type>(key));
        });
  }

public:
  using detail::container_operations<self_type, true>::resolve;

  using detail::container_operations<self_type, true>::append_collection;
  using detail::container_operations<self_type, true>::construct_collection;
  using detail::container_operations<self_type,
                                     true>::construct_collection_by_key;
  using detail::container_operations<self_type, true>::construct;
  using detail::container_operations<self_type, true>::count_collection;

  using detail::container_collections<self_type>::append_collection;
  using detail::container_collections<self_type>::construct_collection;
  using detail::container_collections<self_type>::count_collection;

private:
  friend class detail::container_collections<self_type>;

public:
  using detail::container_operations<self_type, true>::invoke;
  using detail::container_operations<self_type, true>::resolve_missing;
  using detail::container_operations<self_type, true>::resolve_parent;
  using detail::container_operations<self_type, true>::unresolved;

  template <typename Request, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    using request = request_type<Request>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    using lookup_key_type = decltype(key);
    auto status = runtime_registry_.template binding_status<
        typename request::value_type, typename request::exact_type>(key);
    if constexpr (parent_serves_v<typename request::lookup_type,
                                  lookup_key_type>) {
      if (parent_ && status == detail::binding_status::not_found) {
        return parent_->template binding_status<Request>(key);
      }
    }
    return status;
  }

private:
  template <typename R, typename LookupKey>
  std::size_t collection_count(LookupKey key) {
    return this->template count_collection<R>(std::move(key));
  }

  template <typename R, typename LookupKey>
  bool collection_allows_empty(LookupKey key) {
    return runtime_registry_.template has_explicit_collection_lookup<R>(key);
  }

  template <typename R, typename LookupKey>
  R construct_collection_request(construction_scope scope,
                                 runtime_context_type &context, LookupKey key) {
    return this->template construct_collection<R>(
        scope, context, detail::binding_collection_append{}, std::move(key));
  }

  template <typename Context> bool owns_runtime(Context &context) {
    return context.owns(runtime_registry_.runtime());
  }

  auto &runtime_storage() { return runtime_registry_.runtime(); }

  template <typename T, typename LookupKey>
  bool runtime_has_explicit_collection_lookup(LookupKey key) {
    return runtime_registry_.template has_explicit_collection_lookup<T>(key);
  }

  template <typename T, typename LookupKey>
  std::size_t runtime_count(LookupKey key) {
    return runtime_registry_.template count<T>(key);
  }

  template <typename T, typename Fn, typename Context, typename LookupKey>
  std::size_t runtime_collect(construction_scope scope, T &results,
                              Context &context, Fn &&fn, LookupKey key) {
    return runtime_registry_.template collect<T>(scope, results, context,
                                                 std::forward<Fn>(fn), key);
  }

  template <typename Request, typename R, typename Origin, typename LookupKey>
  DINGO_NOINLINE R resolve_nested(construction_scope scope,
                                  runtime_context_type &context, Origin &origin,
                                  LookupKey key) {
    return execute_transaction(
        runtime_registry_.runtime(), context,
        [&](runtime_context_type &local_context) -> R {
          return selection_type::template resolve<Request, R>(
              scope, local_context, origin, std::move(key));
        });
  }

public:
  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(construction_scope scope,
                                runtime_context_type &context, LookupKey key) {
    return resolve<T, RemoveRvalueReferences>(scope, context, *this,
                                              std::move(key));
  }

public:
  runtime_container_impl()
      : runtime_registry_(detail::runtime_data_owner, this) {}

  explicit runtime_container_impl(allocator_type alloc)
      : runtime_registry_(detail::runtime_data_owner, this, alloc) {}

  explicit runtime_container_impl(
      parent_container_type *parent,
      const allocator_type &alloc = allocator_type())
      : runtime_registry_(detail::runtime_data_owner, this, alloc),
        parent_(parent) {}

  runtime_container_impl(const self_type &) = delete;
  runtime_container_impl(self_type &&) = delete;
  self_type &operator=(const self_type &) = delete;
  self_type &operator=(self_type &&) = delete;

private:
  friend class detail::runtime_registration<self_type>;

  runtime_registry_type &binding_store() { return runtime_registry_; }

  runtime_registry_type runtime_registry_;
  parent_container_type *parent_ = nullptr;
};

} // namespace detail

template <typename... Params>
class container : public detail::container_base_t<Params...> {
  using container_base_type = detail::container_base_t<Params...>;

public:
  using container_base_type::container_base_type;
};
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
