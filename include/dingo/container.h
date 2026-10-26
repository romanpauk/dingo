//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/complete_type.h>
#include <dingo/base/config.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution_policy.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/binding/container_miss.h>
#include <dingo/binding/container_selection.h>
#include <dingo/container_base.h>
#include <dingo/container_collections.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/registration.h>
#include <dingo/static/binding.h>
#include <dingo/static/construction.h>
#include <dingo/static/container_traits.h>
#include <dingo/static/graph.h>
#include <dingo/static/registration.h>
#include <dingo/static/registry.h>
#include <dingo/static/resolution_policy.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/request_lift.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename Self, typename RuntimeConfig>
struct static_container_runtime_configuration {
  using runtime_base = runtime_registry<
      typename RuntimeConfig::container_traits_type,
      typename RuntimeConfig::allocator_type,
      typename RuntimeConfig::parent_registry_type,
      std::conditional_t<
          std::is_void_v<typename RuntimeConfig::resolve_root_type>, Self,
          typename RuntimeConfig::resolve_root_type>,
      RuntimeConfig::owns_runtime_data>;

  static constexpr bool owns_runtime_data = RuntimeConfig::owns_runtime_data;
  static constexpr bool merge_parent_collections =
      RuntimeConfig::merge_parent_collections;
  // A runtime-only front-end has no parent registry: the container is the
  // resolve root and consults its parent container directly.
  static constexpr bool runtime_only =
      std::is_void_v<typename RuntimeConfig::parent_registry_type>;
};

template <typename Self>
struct static_container_runtime_configuration<Self, void> {
  using runtime_base =
      runtime_registry<dynamic_container_traits,
                       typename dynamic_container_traits::allocator_type, Self,
                       Self>;

  static constexpr bool owns_runtime_data = true;
  static constexpr bool merge_parent_collections = false;
  static constexpr bool runtime_only = false;
};

} // namespace detail

// One resolve front-end for all container shapes: runtime-only containers
// (container<>, runtime_container) are the form with empty static bindings
// and a runtime-only configuration, container<bindings<...>> adds the static
// registry, and registration-local containers use the merging configuration.
template <typename ParentContainer, typename RuntimeConfig,
          typename... Registrations>
class detail::runtime_container_impl<static_bindings<Registrations...>,
                                     ParentContainer, RuntimeConfig>
    : public detail::runtime_registration<detail::runtime_container_impl<
          static_bindings<Registrations...>, ParentContainer, RuntimeConfig>>,
      public detail::container_collections<detail::runtime_container_impl<
          static_bindings<Registrations...>, ParentContainer, RuntimeConfig>>,
      public detail::container_selection<detail::runtime_container_impl<
          static_bindings<Registrations...>, ParentContainer, RuntimeConfig>>,
      // Empty bindings leave an empty base, so the object does not grow.
      private std::conditional_t<
          (sizeof...(Registrations) != 0),
          detail::static_registry<
              static_bindings<Registrations...>,
              detail::static_registry_state<Registrations...>>,
          none_t> {
  template <typename> friend class runtime_context;
  template <typename> friend class detail::container_selection;
  template <typename, typename, typename>
  friend class detail::runtime_container_impl;
  template <typename Policy, bool MayAutoConstruct, bool StaticError,
            typename Request, typename LookupKey, typename R,
            typename FriendContainer, typename FriendContext>
  friend R resolve_missing_request(FriendContainer &, construction_scope,
                                   FriendContext &, LookupKey);

public:
  using static_bindings_type = static_bindings<Registrations...>;
  using static_registry_type =
      detail::static_registry<static_bindings_type,
                              detail::static_registry_state<Registrations...>>;

private:
  using self_type =
      detail::runtime_container_impl<static_bindings_type, ParentContainer,
                                     RuntimeConfig>;
  using selection_type = detail::container_selection<self_type>;
  using runtime_configuration =
      detail::static_container_runtime_configuration<self_type, RuntimeConfig>;
  using runtime_base = typename runtime_configuration::runtime_base;
  using runtime_context_type =
      runtime_context<typename runtime_base::allocator_type>;
  friend runtime_base;
  template <typename, typename, typename, typename, bool>
  friend class ::dingo::runtime_registry;

  static constexpr bool runtime_only_v = runtime_configuration::runtime_only;
  static constexpr bool has_static_v = sizeof...(Registrations) != 0;

public:
  // A runtime-only container without an explicit parent type accepts a parent
  // of its own type.
  using parent_container_type =
      std::conditional_t<std::is_void_v<ParentContainer> && runtime_only_v,
                         self_type,
                         detail::container_implementation_t<ParentContainer>>;
  using runtime_registry_type = runtime_base;
  using registry_type = runtime_base;
  using container_type = self_type;
  using container_traits_type = typename runtime_base::container_traits_type;
  using allocator_type = typename runtime_base::allocator_type;
  using rtti_type = typename runtime_base::rtti_type;
  using lookup_definition_type = typename runtime_base::lookup_definition_type;

private:
  static constexpr bool has_parent_v = !std::is_void_v<parent_container_type>;

  static_assert(
      detail::static_registration_requirements<
          has_static_v, static_bindings_type,
          typename runtime_base::container_traits_type, RuntimeConfig>::value);

  template <typename T, typename Key>
  using static_selection_t =
      typename static_bindings_type::template selection<T, Key>;

  template <typename T>
  static constexpr bool runtime_auto_constructible_v =
      detail::is_runtime_auto_constructible_dependency_v<T>;

  template <typename Lookup, typename Key>
  static constexpr detail::binding_status static_status_v = [] {
    if constexpr (has_static_v) {
      return static_selection_t<Lookup, Key>::status;
    } else {
      return detail::binding_status::not_found;
    }
  }();

  // Runtime-only containers fall back to the parent only for lookups the
  // parent declares; containers with static bindings always do.
  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves() {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    if constexpr (runtime_only_v) {
      return has_parent_v &&
             detail::has_lookup<parent_container_type, Lookup, LookupKey>();
    } else {
      return has_parent_v;
    }
  }

  using runtime_selection = typename runtime_registry_type::runtime_selection;

  static constexpr bool runtime_only = runtime_only_v;
  static constexpr bool has_parent = has_parent_v;
  static constexpr bool has_static_source = has_static_v;
  static constexpr bool has_runtime_source = true;
  static constexpr bool static_binding_uses_result = true;

  template <typename T>
  static constexpr bool may_auto_construct_v = runtime_auto_constructible_v<T>;

  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves_v = parent_serves<Lookup, LookupKey>();

  runtime_registry_type &runtime_source() { return runtime_registry_; }
  static_registry_type &static_source() { return statics(); }
  parent_container_type *parent() { return parent_; }

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_binding(LookupKey key) {
    return runtime_registry_.template binding_status<
               typename request_type<Request>::value_type,
               typename request_type<Request>::exact_type>(key) !=
           detail::binding_status::not_found;
  }

  template <typename Request, typename NormalizedRequest>
  bool has_runtime_no_key_binding() {
    auto key = detail::no_lookup_key();
    static_assert(std::is_same_v<request_value_t<Request>,
                                 request_value_t<NormalizedRequest>>);
    return has_runtime_binding<Request>(key);
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_collection(LookupKey key) {
    return runtime_registry_.template count_collection<T>(key) != 0;
  }

  template <typename Request, typename Key,
            bool Found =
                static_status_v<Request, Key> == detail::binding_status::found>
  struct is_static_binding_available : std::false_type {};

  template <typename Request, typename Key>
  struct is_static_binding_available<Request, Key, true> {
    using binding = typename static_selection_t<Request, Key>::binding_type;
    static constexpr bool value =
        detail::binding_supports_request_v<request_result_t<Request>, binding>;
  };

  template <typename Request, typename Key>
  static constexpr bool is_static_binding_available_v =
      is_static_binding_available<Request, Key>::value;
  template <typename T>
  static constexpr bool has_static_construct_v = [] {
    if constexpr (has_static_v) {
      using request = request_type<T>;
      using interface_type = typename request::interface_type;
      using value_type = typename request::value_type;
      using no_key = detail::no_lookup_key_t;
      constexpr bool has_exact_static_binding =
          is_static_binding_available_v<interface_type, no_key>;
      constexpr bool has_normalized_static_binding =
          is_static_binding_available_v<value_type, no_key>;

      return has_exact_static_binding || (has_normalized_static_binding &&
                                          construct_normalized_request_v<T>);
    } else {
      return false;
    }
  }();

  template <typename T, typename Key>
  bool select_static_collection_construct() {
    constexpr bool has_static_collection_bindings =
        detail::static_collection_binding_count<static_bindings_type, T,
                                                Key>() != 0;
    if constexpr (!has_static_collection_bindings) {
      return false;
    } else {
      return !has_runtime_collection<T>(Key{});
    }
  }

  template <typename Request, typename R = typename Request::result_type>
  DINGO_ALWAYS_INLINE R construct_static(construction_scope scope,
                                         runtime_context_type &context) {
    using interface_type = typename Request::interface_type;
    using request_value_type = typename Request::value_type;
    using no_key = detail::no_lookup_key_t;
    using exact_selection = static_selection_t<interface_type, no_key>;
    using normalized_selection = static_selection_t<request_value_type, no_key>;
    return detail::construct_from_static_source<
        Request, R, runtime_context_type, false, exact_selection,
        normalized_selection>(
        scope, context,
        [&](auto) { return resolve_static<Request, no_key>(scope, context); },
        [&](auto) {
          return resolve_static<request_type<request_value_type>, no_key>(
              scope, context);
        },
        [&](auto) { return resolve_static<Request, no_key>(scope, context); });
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_impl(construction_scope scope,
                              runtime_context_type &context, LookupKey key,
                              Fn &&fn) {
    // A declared collection lookup with no rows is a valid empty collection.
    return detail::construct_binding_collection<T>(
        count_collection<T>(key),
        runtime_registry_.template has_explicit_collection_lookup<T>(key),
        [&](T &results) {
          append_collection_impl<T>(scope, results, context, key, fn);
        });
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    return construct_collection_impl<T>(scope, context, std::move(key),
                                        std::forward<Fn>(fn));
  }

  static_registry_type &statics() { return *this; }

  // A request no binding of this container answers goes to the parent, when
  // the parent can serve it, and is reported unresolved otherwise.
  template <typename Request, typename R, bool MayAutoConstruct,
            bool ParentServes, typename Origin, typename LookupKey>
  R resolve_missing(construction_scope scope, runtime_context_type &context,
                    Origin &origin, LookupKey key) {
    if constexpr (ParentServes) {
      if (parent_) {
        return resolve_parent<Request>(scope, context, origin, key);
      }
    }
    return origin.template unresolved<Request, MayAutoConstruct, LookupKey, R>(
        scope, context, key);
  }

  template <typename Request, typename LookupKey, typename Context,
            typename R = typename Request::result_type>
  DINGO_ALWAYS_INLINE R resolve_static(construction_scope scope,
                                       Context &context) {
    return statics().template resolve_request<Request, LookupKey>(
        scope, context, *this);
  }

  template <typename Request, typename Origin, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(construction_scope scope, runtime_context_type &context,
                   Origin &origin, LookupKey key) {
    using user_type = typename Request::user_type;
    return parent_
        ->template resolve<user_type, Request::removes_rvalue_references>(
            scope, context, origin, std::move(key));
  }

  template <typename Request, typename R = typename Request::lookup_type>
  R resolve_request(construction_scope scope, runtime_context_type &context) {
    auto key = detail::no_lookup_key();
    auto selected = runtime_registry_.template select_binding<
        typename Request::value_type, typename Request::exact_type>(key);
    return detail::resolve_selection<R, typename Request::lookup_type>(
        selected, context,
        [&]() -> R {
          return runtime_registry_
              .template resolve_binding<typename Request::interface_type, R>(
                  selected, scope, context);
        },
        [&]() -> R {
          return unresolved<
              Request,
              runtime_auto_constructible_v<typename Request::user_type>,
              detail::no_lookup_key_t, R>(scope, context, std::move(key));
        });
  }

  template <typename Request, typename R = typename Request::result_type>
  R construct_runtime_resolved_request(construction_scope scope,
                                       runtime_context_type &context) {
    try {
      return resolve_request<Request>(scope, context);
    } catch (const type_not_convertible_exception &) {
      using request_value_type = typename Request::value_type;
      auto &&value =
          resolve_request<request_type<request_value_type>>(scope, context);
      return type_traits<std::decay_t<typename Request::user_type>>::make(
          std::forward<decltype(value)>(value));
    }
  }

  template <typename Request,
            typename Factory = constructor<typename Request::value_type>,
            typename R = typename Request::result_type>
  R construct_runtime_request(construction_scope scope,
                              runtime_context_type &context,
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
          return resolve_request<Request>(scope, context);
        } else {
          return construct_runtime_resolved_request<Request, R>(scope, context);
        }
      }
    }

    if constexpr (construct_factory_request_v<user_type>) {
      auto type_guard = context.template track_type<request_value_type>();
      return factory.template construct<R>(scope, context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             user_type>) {
      ::dingo::throw_missing_rvalue_conversion<user_type>(false, context);
    } else {
      return resolve_request<Request>(scope, context);
    }
  }

  // Runtime-only containers resolve through their parent when constructing.
  template <typename Request, typename R = typename Request::result_type>
  R construct_resolved_request(construction_scope scope,
                               runtime_context_type &context) {
    using user_type = typename Request::user_type;
    try {
      return selection_type::template resolve<Request, R>(
          scope, context, *this, detail::no_lookup_key());
    } catch (const type_not_convertible_exception &) {
      using request_value_type = typename Request::value_type;
      using normalized = request_type<request_value_type>;
      auto &&value =
          selection_type::template resolve<normalized,
                                           typename normalized::lookup_type>(
              scope, context, *this, detail::no_lookup_key());
      return type_traits<std::decay_t<user_type>>::make(
          std::forward<decltype(value)>(value));
    }
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
      if (binding_status<typename Request::lookup_type>(key) !=
          detail::binding_status::not_found) {
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

public:
  template <bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<OwnsRuntimeData, int> = 0>
  runtime_container_impl()
      : runtime_registry_(detail::runtime_data_owner, this) {}

  template <bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<OwnsRuntimeData, int> = 0>
  explicit runtime_container_impl(allocator_type alloc)
      : runtime_registry_(detail::runtime_data_owner, this, alloc) {}

  template <bool RuntimeOnly = runtime_only_v,
            std::enable_if_t<RuntimeOnly, int> = 0>
  runtime_container_impl(parent_container_type *parent,
                         const allocator_type &alloc = allocator_type())
      : runtime_registry_(detail::runtime_data_owner, this, alloc),
        parent_(parent) {}

  template <typename Parent = ParentContainer,
            bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<!std::is_void_v<Parent> && OwnsRuntimeData &&
                                 !runtime_only_v,
                             int> = 0>
  explicit runtime_container_impl(Parent *parent,
                                  allocator_type alloc = allocator_type())
      : runtime_registry_(detail::runtime_data_owner, this, alloc),
        parent_(detail::container_implementation_of(parent)) {}

  template <
      typename Parent = ParentContainer,
      bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
      std::enable_if_t<!std::is_void_v<Parent> && !OwnsRuntimeData, int> = 0>
  explicit runtime_container_impl(Parent *parent,
                                  allocator_type alloc = allocator_type())
      : runtime_registry_(parent, alloc), parent_(parent) {}

  // Compile-time bindings keep their instances inline, resolved instances
  // reference each other by address, and runtime bindings point back at the
  // container, so it cannot move. runtime_container moves by owning it through
  // a handle.
  runtime_container_impl(const self_type &) = delete;
  runtime_container_impl(self_type &&) = delete;
  self_type &operator=(const self_type &) = delete;
  self_type &operator=(self_type &&) = delete;

  self_type &container() { return *this; }

  const self_type &container() const { return *this; }

private:
  template <typename Request, typename R, typename LookupKey>
  static constexpr bool is_cacheable() {
    if constexpr (!detail::cache::supports_v<
                      typename Request::interface_type> ||
                  collection_traits<R>::is_collection) {
      return false;
    } else if constexpr (has_static_v &&
                         detail::is_static_lookup_key_definition_v<LookupKey>) {
      return static_status_v<typename Request::lookup_type, LookupKey> ==
             detail::binding_status::not_found;
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
          return runtime_registry_.template resolve_binding<interface_type, R>(
              selected, ephemeral_scope, context);
        });
  }

  template <typename Request, typename R, typename LookupKey>
  R resolve_entry_direct(LookupKey key) {
    using interface_type = typename Request::interface_type;
    if constexpr (is_cacheable<Request, R, LookupKey>()) {
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
            // MSVC's pre-C++20 lambda processor does not treat an enclosing
            // constexpr local as a constant expression, so spell it here.
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
        detail::resolution_policy<user_type, LookupKey, static_bindings_type,
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
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T, true>,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    using request = request_type<T>;
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T, true>>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    using request = request_type<T>;
    using interface_type = typename request::interface_type;
    using value_type = typename request::value_type;
    if constexpr (runtime_only_v) {
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            return construct_request<request, Factory, R>(
                ephemeral_scope, context, std::move(factory));
          });
    } else {
      if constexpr (std::is_same_v<Factory,
                                   constructor<normalized_type_t<T>>>) {
        const bool has_runtime_no_key =
            has_runtime_no_key_binding<interface_type, value_type>();
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          using no_key = detail::no_lookup_key_t;
          constexpr bool has_static_normalized_binding =
              is_static_binding_available_v<value_type, no_key>;

          if constexpr (has_static_construct_v<T>) {
            if (!has_runtime_no_key) {
              return execute_transaction(
                  runtime_registry_.runtime(),
                  [&](runtime_context_type &context) -> R {
                    return construct_static<request, R>(ephemeral_scope,
                                                        context);
                  });
            }
          }

          if (has_runtime_no_key) {
            return execute_transaction(
                runtime_registry_.runtime(),
                [&](runtime_context_type &context) -> R {
                  return construct_runtime_request<request, Factory, R>(
                      ephemeral_scope, context, std::move(factory));
                });
          }

          if constexpr (has_static_normalized_binding) {
            ::dingo::throw_missing_rvalue_conversion<T>(true);
          }

          return execute_transaction(
              runtime_registry_.runtime(),
              [&](runtime_context_type &context) -> R {
                return construct_runtime_request<request, Factory, R>(
                    ephemeral_scope, context, std::move(factory));
              });
        } else if constexpr (has_static_construct_v<T>) {
          if (!has_runtime_no_key) {
            return execute_transaction(runtime_registry_.runtime(),
                                       [&](runtime_context_type &context) -> R {
                                         return construct_static<request, R>(
                                             ephemeral_scope, context);
                                       });
          }
        } else {
          return execute_transaction(
              runtime_registry_.runtime(),
              [&](runtime_context_type &context) -> R {
                return construct_runtime_request<request, Factory, R>(
                    ephemeral_scope, context, std::move(factory));
              });
        }
      }
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            if constexpr (std::is_same_v<Factory,
                                         constructor<normalized_type_t<T>>>) {
              auto key = detail::no_lookup_key();
              const auto status = binding_status<T>(key);
              if (status != detail::binding_status::not_found) {
                if constexpr (construct_normalized_request_v<T>) {
                  return ::dingo::construct_resolved_request<T>(
                      [&]() {
                        return selection_type::template resolve<
                            request, typename request::lookup_type>(
                            ephemeral_scope, context, *this, key);
                      },
                      [&]() {
                        using normalized_request = request_type<value_type>;
                        return selection_type::template resolve<
                            normalized_request,
                            typename normalized_request::lookup_type>(
                            ephemeral_scope, context, *this, key);
                      });
                } else {
                  return selection_type::template resolve<
                      request, typename request::lookup_type>(
                      ephemeral_scope, context, *this, key);
                }
              } else if (binding_status<value_type>(key) !=
                         detail::binding_status::not_found) {
                if constexpr (construct_normalized_request_v<T>) {
                  using normalized_request = request_type<value_type>;
                  return type_traits<std::decay_t<T>>::make(
                      selection_type::template resolve<
                          normalized_request,
                          typename normalized_request::lookup_type>(
                          ephemeral_scope, context, *this, key));
                } else {
                  return selection_type::template resolve<
                      request, typename request::lookup_type>(
                      ephemeral_scope, context, *this, key);
                }
              }
            }

            if constexpr (construct_factory_request_v<T>) {
              auto type_guard = context.template track_type<value_type>();
              return factory.template construct<R>(ephemeral_scope, context,
                                                   *this);
            } else {
              return selection_type::template resolve<
                  request, typename request::lookup_type>(
                  ephemeral_scope, context, *this, detail::no_lookup_key());
            }
          });
    }
  }

  using detail::container_collections<self_type>::append_collection;
  using detail::container_collections<self_type>::construct_collection;
  using detail::container_collections<self_type>::count_collection;

private:
  friend class detail::container_collections<self_type>;

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_by_key(LookupKey key) {
    if constexpr (has_static_v && detail::is_static_lookup_key_v<LookupKey>) {
      if (select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            runtime_registry_.runtime(),
            [&](runtime_context_type &context) -> T {
              return statics().template construct_collection<T, LookupKey>(
                  ephemeral_scope, *this, context);
            });
      }
    }

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> T {
          return construct_collection<T>(ephemeral_scope, context,
                                         detail::binding_collection_append{},
                                         std::move(key));
        });
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_by_key(Fn &&fn, LookupKey key) {
    if constexpr (has_static_v && detail::is_static_lookup_key_v<LookupKey>) {
      if (select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            runtime_registry_.runtime(),
            [&](runtime_context_type &context) -> T {
              return statics().template construct_collection<T, LookupKey>(
                  ephemeral_scope, *this, context, std::forward<Fn>(fn));
            });
      }
    }

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> T {
          return construct_collection<T>(ephemeral_scope, context,
                                         std::forward<Fn>(fn), std::move(key));
        });
  }

public:
  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    return execute_transaction(runtime_registry_.runtime(),
                               [&](runtime_context_type &context) {
                                 return detail::container_invoke<Signature>(
                                     context, *this, ephemeral_scope,
                                     std::forward<Callable>(callable));
                               });
  }

  template <typename Request, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    using request = request_type<Request>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    using lookup_key_type = decltype(key);
    if constexpr (runtime_only_v) {
      auto status = runtime_registry_.template binding_status<
          typename request::value_type, typename request::exact_type>(key);
      if constexpr (parent_serves<typename request::lookup_type,
                                  lookup_key_type>()) {
        if (parent_ && status == detail::binding_status::not_found) {
          return parent_->template binding_status<Request>(key);
        }
      }
      return status;
    } else {
      using interface_type = typename request::interface_type;
      const auto runtime_status = runtime_registry_.template binding_status<
          typename request_type<interface_type>::value_type,
          typename request_type<interface_type>::exact_type>(key);
      return detail::resolve_binding_status<
          static_status_v<interface_type, lookup_key_type>>(
          runtime_status,
          detail::binding_resolution_policy::ambiguous_on_conflict);
    }
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return append_collection_impl<T>(scope, results, context, std::move(key),
                                     std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection_impl(construction_scope scope, T &results,
                                     runtime_context_type &context,
                                     LookupKey key, Fn &&fn) {
    std::size_t count = 0;
    if constexpr (runtime_configuration::merge_parent_collections &&
                  has_parent_v) {
      if (parent_) {
        count += parent_->template append_collection<T>(scope, results, context,
                                                        fn, key);
      }
    }
    if constexpr (has_static_v) {
      count += detail::append_binding_collection(
          results,
          [&](auto &collection, auto &&append) {
            return runtime_registry_.template append_collection<T>(
                scope, collection, context,
                std::forward<decltype(append)>(append), key);
          },
          [&](auto &collection, auto &&append) {
            return statics().template append_collection<T, LookupKey>(
                scope, collection, *this, context,
                std::forward<decltype(append)>(append));
          },
          std::forward<Fn>(fn));
    } else {
      count += runtime_registry_.template append_collection<T>(
          scope, results, context, std::forward<Fn>(fn), key);
    }
    return count;
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    std::size_t count = runtime_registry_.template count_collection<T>(key);
    if constexpr (has_static_v) {
      count += static_registry_type::template count_collection<T, LookupKey>();
    }
    if constexpr (runtime_configuration::merge_parent_collections &&
                  has_parent_v) {
      if (parent_) {
        count += parent_->template count_collection<T>(key);
      }
    }
    return count;
  }

private:
  template <typename R, typename LookupKey>
  std::size_t collection_count(LookupKey key) {
    return count_collection<R>(std::move(key));
  }

  template <typename R, typename LookupKey>
  bool collection_allows_empty(LookupKey key) {
    return runtime_registry_.template has_explicit_collection_lookup<R>(key);
  }

  template <typename R, typename LookupKey>
  R construct_collection_request(construction_scope scope,
                                 runtime_context_type &context, LookupKey key) {
    return construct_collection<R>(
        scope, context, detail::binding_collection_append{}, std::move(key));
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

  template <typename T, bool RemoveRvalueReferences, typename Origin,
            typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(construction_scope scope,
                                runtime_context_type &context, Origin &origin,
                                LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    using policy =
        detail::resolution_policy<T, LookupKey, static_bindings_type,
                                  detail::resolution_mode::runtime_fallback>;
    if constexpr (!policy::lift_enabled) {
      if (context.owns(runtime_registry_.runtime())) {
        return selection_type::template resolve<request, R>(
            scope, context, origin, std::move(key));
      }
      return resolve_nested<request, R>(scope, context, origin, std::move(key));
    }
    return policy::template resolve<R>(
        [&](auto) -> R {
          if (context.owns(runtime_registry_.runtime())) {
            return selection_type::template resolve<request, R>(
                scope, context, origin, std::move(key));
          }
          return resolve_nested<request, R>(scope, context, origin,
                                            std::move(key));
        },
        [&](auto tag) -> R {
          using selected_policy = typename decltype(tag)::policy;
          return selected_policy::lift_type::make(
              resolve<typename selected_policy::inner_type, false>(
                  scope, context, origin, key));
        });
  }

  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            bool StaticError = false,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, runtime_context_type &context,
               LookupKey key) {
    return detail::resolve_missing_request<
        detail::runtime_miss_policy<runtime_only_v>, MayAutoConstruct,
        StaticError, Request, LookupKey, R>(*this, scope, context,
                                            std::move(key));
  }

private:
  friend class detail::runtime_registration<self_type>;

  runtime_registry_type &binding_store() { return runtime_registry_; }

  self_type &runtime_registration_parent() { return *this; }

  runtime_registry_type runtime_registry_;
  parent_container_type *parent_ = nullptr;
};

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
