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
#include <dingo/base/type_list.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_model.h>
#include <dingo/binding/binding_resolution_policy.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/factory/callable.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/operations.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/runtime/registration.h>
#include <dingo/runtime/resolution.h>
#include <dingo/runtime/selection.h>
#include <dingo/runtime/state.h>
#include <dingo/runtime/transaction.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace dingo {
namespace detail {

template <typename StaticRegistry, typename ParentContainer,
          typename RuntimeConfig>
class container_with_static_bindings;

// Keep constructibility queries in the non-reference specialization. MSVC
// otherwise evaluates its constructibility intrinsics for incomplete reference
// targets while instantiating the dispatch trait.
template <typename T,
          bool IsNormalized =
              std::is_same_v<request_value_t<T>, std::decay_t<T>>,
          bool IsReference = std::is_reference_v<T>>
struct is_runtime_auto_constructible_dependency : std::false_type {};

template <typename T>
struct is_runtime_auto_constructible_dependency<T, true, false>
    : std::bool_constant<std::is_move_constructible_v<std::decay_t<T>> ||
                         std::is_copy_constructible_v<std::decay_t<T>>> {};

template <typename T>
struct is_runtime_auto_constructible_dependency<T, true, true>
    : std::bool_constant<std::is_lvalue_reference_v<T> &&
                         std::is_const_v<std::remove_reference_t<T>> &&
                         is_auto_constructible<std::decay_t<T>>::value> {};

template <typename T>
inline constexpr bool is_runtime_auto_constructible_dependency_v =
    is_runtime_auto_constructible_dependency<T>::value;

template <typename ContainerTraits, typename Allocator>
class registry_store
    : protected runtime_registration_store<ContainerTraits, Allocator> {
protected:
  using lookup_base = runtime_lookup_store<ContainerTraits, Allocator>;
  using typename lookup_base::rtti_type;
  using typename lookup_base::runtime_bindings_state;
  using typename lookup_base::runtime_context_type;
  using typename lookup_base::runtime_selection;

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(runtime_bindings_state *state,
                         construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    return construct_binding_collection<T>(
        this->template count_collection<T>(state, key),
        this->template has_explicit_collection_lookup<T>(key), [&](T &results) {
          append_runtime_collection(state, results, scope, context,
                                    std::forward<Fn>(fn), std::move(key));
        });
  }

  template <typename Request, typename Result = Request>
  Result resolve_binding(runtime_selection selection, construction_scope scope,
                         runtime_context_type &context) {
    auto &binding = *selection.binding;
    if constexpr (detail::cache::supports_v<Request>) {
      return resolve<Request, Result>(scope, binding, context,
                                      detail::cache::key<Request>());
    } else {
      return resolve<Request, Result>(scope, binding, context);
    }
  }

  struct cache_result {
    bool hit = false;
    void *address = nullptr;
  };

  template <typename Request>
  cache_result lookup_cache(runtime_selection selection) const {
    if constexpr (detail::cache::supports_v<Request>) {
      if (selection.status == detail::binding_status::found) {
        auto *entry = selection.binding->cache_slot();
        if (entry != nullptr && entry->key == detail::cache::key<Request>()) {
          return {true, entry->address};
        }
      }
    }
    return {};
  }

  template <typename Request, typename Result>
  static Result resolve_cached(cache_result result) {
    assert(result.hit);
    return detail::convert_resolved_binding<Request>(result.address);
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_runtime_collection(runtime_bindings_state *state,
                                        T &results, construction_scope scope,
                                        runtime_context_type &context, Fn &&fn,
                                        LookupKey key) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    if (!state) {
      return 0;
    }
    return this->template for_each_collection_entry<T>(
        *state, key, [&](auto &entry) {
          fn(results, resolve_collection_type<resolve_type>(
                          scope, entry.binding(), context));
        });
  }

  template <typename Request, typename Result, typename Binding,
            typename Context>
  Result resolve(construction_scope scope, Binding &binding, Context &context,
                 const void *cache_key = nullptr) {
    return ::dingo::resolve_binding_request<Request, rtti_type>(
        scope, binding, context, cache_key);
  }

  template <typename T, typename Binding, typename Context>
  T resolve_collection_type(construction_scope scope, Binding &binding,
                            Context &context) {
    return ::dingo::resolve_binding_request<T, rtti_type>(scope, binding,
                                                          context);
  }

  registry_store() = default;
  registry_store(const registry_store &) = delete;
  registry_store &operator=(const registry_store &) = delete;
  ~registry_store() = default;
};

} // namespace detail

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot, bool OwnsRuntimeData = true>
class DINGO_EMPTY_BASES runtime_registry
    : public allocator_base<Allocator>,
      public detail::runtime_registration_api<
          runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                           ResolveRoot, OwnsRuntimeData>>,
      public detail::runtime_registration<
          runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                           ResolveRoot, OwnsRuntimeData>>,
      protected detail::registry_store<ContainerTraits, Allocator> {
  template <typename> friend class runtime_context;
  template <typename> friend class detail::context_resolution;
  template <typename> friend class detail::runtime_registration_api;
  template <typename> friend class detail::runtime_registration;
  template <typename, typename, typename, typename>
  friend class runtime_binding_state;
  template <typename, typename, typename>
  friend class detail::container_with_static_bindings;
  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT,
            bool OwnsRuntimeDataT>
  friend class runtime_registry;
  using store_type = detail::registry_store<ContainerTraits, Allocator>;
  using registry_type =
      runtime_registry<ContainerTraits, Allocator, ParentRegistry, ResolveRoot,
                       OwnsRuntimeData>;
  using resolve_root_type = ResolveRoot;
  using container_type = resolve_root_type;
  using runtime_type = container_runtime<Allocator>;
  using typename store_type::runtime_binding_interface_type;
  using typename store_type::runtime_context_type;
  using typename store_type::runtime_selection;
  using typename store_type::runtime_transaction_type;
  template <typename Parent>
  using registration_runtime_config = detail::static_container_runtime_config<
      ContainerTraits, Allocator, Parent, resolve_root_type, false, true>;
  template <typename Parent>
  using runtime_registration_container_type =
      runtime_registry<ContainerTraits, Allocator, Parent, resolve_root_type,
                       false>;
  template <typename Bindings, typename Parent>
  using registration_container_type = std::conditional_t<
      std::is_void_v<Bindings>, runtime_registration_container_type<Parent>,
      detail::container_with_static_bindings<
          Bindings, Parent, registration_runtime_config<Parent>>>;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using parent_container_type =
      std::conditional_t<std::is_same_v<void, ParentRegistry>,
                         resolve_root_type, ParentRegistry>;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;

protected:
  using typename store_type::base_lookup_entry;
  using typename store_type::runtime_bindings_state;
  using typename store_type::runtime_data_type;
  using typename store_type::runtime_lookup_binding_view;
  using typename store_type::runtime_lookup_value;
  using typename store_type::runtime_scope_state;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      typename store_type::template lookup_entry_cardinality<LookupEntry>;
  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename store_type::template runtime_lookup_entries_for_interface_t<
          TypeInterface>;
  template <typename Interface, typename Key>
  using lookup_index_route =
      typename store_type::template lookup_index_route<Interface, Key>;
  template <typename... Bindings>
  using runtime_binding_value_impl =
      typename store_type::template runtime_binding_value_impl<Bindings...>;
  template <typename Owner>
  using shared_lookup_value_factory =
      typename store_type::template shared_lookup_value_factory<Owner>;
  template <bool SharedOwner, typename Binding>
  using direct_lookup_value_factory =
      typename store_type::template direct_lookup_value_factory<SharedOwner,
                                                                Binding>;

private:
  static runtime_data_type &borrow_runtime_data(resolve_root_type *root) {
    auto &data = root->binding_store().shared_runtime_data();
    static_assert(
        std::is_same_v<std::remove_reference_t<decltype(data)>,
                       runtime_data_type>,
        "registration container traits must preserve the root runtime lookup "
        "and allocator configuration");
    return data;
  }

public:
  runtime_registry(detail::runtime_data_owner_t, parent_container_type *parent,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), parent_(parent),
        runtime_data_(get_allocator()) {
    static_assert(OwnsRuntimeData);
  }

  template <bool Enabled = OwnsRuntimeData, std::enable_if_t<!Enabled, int> = 0>
  runtime_registry(parent_container_type *parent,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), parent_(parent),
        runtime_data_() {}

private:
  registry_type &binding_store() { return *this; }

  registry_type &runtime_registration_parent() { return *this; }

public:
  runtime_registry(const runtime_registry &) = delete;
  runtime_registry &operator=(const runtime_registry &) = delete;

  ~runtime_registry() = default;

  allocator_type &get_allocator() {
    return allocator_base<allocator_type>::get_allocator();
  }

protected:
  container_runtime<allocator_type> &runtime() {
    return shared_runtime_data().runtime;
  }

  runtime_data_type &shared_runtime_data() {
    if constexpr (OwnsRuntimeData) {
      return runtime_data_.get();
    } else {
      return borrow_runtime_data(resolve_root());
    }
  }

  runtime_bindings_state *runtime_bindings() {
    if constexpr (OwnsRuntimeData) {
      return std::addressof(shared_runtime_data().bindings);
    } else {
      auto *scope = runtime_scope();
      return scope != nullptr ? std::addressof(scope->bindings) : nullptr;
    }
  }

  runtime_scope_state *runtime_scope() {
    static_assert(!OwnsRuntimeData);
    const auto scope = runtime_data_.scope_id();
    if (scope == decltype(runtime_data_)::invalid_scope) {
      return nullptr;
    }
    return shared_runtime_data().find_scope(scope);
  }

  void release_runtime_scope(runtime_scope_state *state) noexcept {
    shared_runtime_data().release_scope(runtime_data_.scope_id(), state);
    runtime_data_.scope_id(decltype(runtime_data_)::invalid_scope);
  }

  template <typename Parent>
  runtime_bindings_state &
  ensure_runtime_bindings(runtime_transaction_type &transaction,
                          Parent *registration_parent) {
    if constexpr (OwnsRuntimeData) {
      return *runtime_bindings();
    } else {
      if (auto *scope = runtime_scope()) {
        assert(scope->registration_parent == registration_parent);
        return scope->bindings;
      }

      auto &data = shared_runtime_data();
      auto *state =
          transaction.template construct_persistent<runtime_scope_state>(
              get_allocator(), registration_parent);
      runtime_data_.scope_id(data.create_scope(state));
      try {
        transaction.on_rollback(
            +[](void *self, void *scope) noexcept {
              static_cast<registry_type *>(self)->release_runtime_scope(
                  static_cast<runtime_scope_state *>(scope));
            },
            this, state);
      } catch (...) {
        release_runtime_scope(state);
        throw;
      }
      return state->bindings;
    }
  }

  template <typename Parent> Parent *runtime_registration_parent() {
    if constexpr (std::is_same_v<Parent, parent_container_type>) {
      return parent();
    } else {
      static_assert(!OwnsRuntimeData);
      auto *scope = runtime_scope();
      assert(scope != nullptr && scope->registration_parent != nullptr);
      return static_cast<Parent *>(scope->registration_parent);
    }
  }

  parent_container_type *parent() { return parent_; }

  resolve_root_type *resolve_root() {
    if constexpr (std::is_same_v<resolve_root_type, registry_type>) {
      return this;
    } else if constexpr (std::is_base_of_v<registry_type, resolve_root_type>) {
      return static_cast<resolve_root_type *>(this);
    } else if constexpr (std::is_same_v<parent_container_type,
                                        resolve_root_type>) {
      return parent_;
    } else {
      assert(parent_ != nullptr);
      return parent_->binding_store().resolve_root();
    }
  }

  const resolve_root_type *resolve_root() const {
    return const_cast<registry_type *>(this)->resolve_root();
  }

protected:
  // Lookup misses are answered by the parent chain instead of failing at
  // compile time when the registry is not the resolve root.
  static constexpr bool lookup_may_miss() {
    if constexpr (std::is_same_v<void, ParentRegistry>) {
      return false;
    } else {
      return !std::is_base_of_v<registry_type, resolve_root_type>;
    }
  }

  template <typename Value, typename Exact, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  runtime_selection select_binding(LookupKey key) {
    return store_type::template source_select<Value, Exact, lookup_may_miss()>(
        runtime_bindings(), std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    return store_type::template construct_collection<T>(
        runtime_bindings(), scope, context, std::forward<Fn>(fn),
        std::move(key));
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    return store_type::template count_collection<T>(runtime_bindings(),
                                                    std::move(key));
  }

  template <typename T, typename Fn>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn) {
    return store_type::append_runtime_collection(
        runtime_bindings(), results, scope, context, std::forward<Fn>(fn),
        detail::no_lookup_key());
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return store_type::append_runtime_collection(
        runtime_bindings(), results, scope, context, std::forward<Fn>(fn),
        std::move(key));
  }

  template <typename Value, typename Exact, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(LookupKey key) {
    return store_type::template binding_status<Value, Exact, lookup_may_miss()>(
        runtime_bindings(), std::move(key));
  }

protected:
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R source_missing(construction_scope scope, runtime_context_type &context,
                   const LookupKey &key) {
    using user_type = typename Request::user_type;
    using lookup_key_type = std::decay_t<LookupKey>;

    if constexpr (MayAutoConstruct && collection_traits<R>::is_collection) {
      if (this->template count_collection<R>(key) != 0) {
        return this->template construct_collection<R>(
            scope, context, detail::binding_collection_append{}, key);
      }
    }

    if constexpr (!std::is_same_v<void, ParentRegistry> &&
                  !std::is_base_of_v<registry_type, resolve_root_type>) {
      if (parent_) {
        return parent()
            ->template resolve<user_type, Request::removes_rvalue_references>(
                scope, context, key);
      }
    }

    if constexpr (MayAutoConstruct &&
                  detail::is_static_lookup_key_definition_v<lookup_key_type> &&
                  !detail::is_no_lookup_key_v<lookup_key_type> &&
                  collection_traits<R>::is_collection) {
      return this->template construct_collection<R>(
          scope, context, detail::binding_collection_append{}, key);
    } else {
      return detail::construct_unbound_request<Request, MayAutoConstruct, R>(
          scope, context, *resolve_root(), key);
    }
  }

public:
  template <typename TypeInterface, typename TypeStorage, typename BindingState,
            typename LookupKey>
  // Lookup keys select an index route; they do not change binding behavior.
  // Excluding them from the binding identity shares this specialization.
  using runtime_registration_binding_t =
      runtime_binding<container_type,
                      typename annotated_traits<TypeInterface>::type,
                      TypeStorage, BindingState>;

  template <typename InterfaceList, typename TypeStorage, typename BindingState,
            typename LookupKey>
  struct runtime_binding_value_owner;

  template <typename... TypeInterfaces, typename TypeStorage,
            typename BindingState, typename LookupKey>
  struct runtime_binding_value_owner<type_list<TypeInterfaces...>, TypeStorage,
                                     BindingState, LookupKey> {
    using type = runtime_binding_value_impl<runtime_registration_binding_t<
        TypeInterfaces, TypeStorage, BindingState, LookupKey>...>;
  };

  template <typename TypeInterface, typename TypeStorage, typename BindingState,
            typename LookupKey>
  struct runtime_binding_value_owner<type_list<TypeInterface>, TypeStorage,
                                     BindingState, LookupKey> {
    using type = runtime_registration_binding_t<TypeInterface, TypeStorage,
                                                BindingState, LookupKey>;
  };

  template <typename InterfaceList, typename TypeStorage, typename BindingState,
            typename LookupKey>
  using runtime_binding_value_owner_t =
      typename runtime_binding_value_owner<InterfaceList, TypeStorage,
                                           BindingState, LookupKey>::type;

private:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type>
  R resolve_impl(construction_scope scope, runtime_context_type &context,
                 LookupKey key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    using Type = typename Request::value_type;
    static_assert(!std::is_const_v<Type>);

    auto selection = select_binding<typename Request::value_type,
                                    typename Request::exact_type>(key);
    return detail::resolve_selection<R, typename Request::lookup_type>(
        selection, context,
        [&]() -> R {
          return this->template resolve_binding<
              typename Request::lookup_type, typename Request::interface_type>(
              selection, scope, context);
        },
        [&]() -> R {
          return source_missing<Request, MayAutoConstruct, LookupKey,
                                request_result_t<R>>(scope, context, key);
        });
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    return resolve_impl<request,
                        detail::is_runtime_auto_constructible_dependency_v<
                            typename request::user_type>>(scope, context,
                                                          std::move(key));
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  parent_container_type *parent_ = nullptr;

  detail::runtime_data_holder<runtime_data_type, OwnsRuntimeData> runtime_data_;
};

} // namespace dingo
