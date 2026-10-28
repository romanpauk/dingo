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
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/runtime/registration.h>
#include <dingo/runtime/registration_store.h>
#include <dingo/runtime/selection.h>
#include <dingo/runtime/storage.h>
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

template <typename Derived> class runtime_registration;

template <typename StaticRegistry, typename ParentContainer,
          typename RuntimeConfig>
class runtime_container_impl;

template <typename Self> class container_selection;

struct runtime_data_owner_t {};
inline constexpr runtime_data_owner_t runtime_data_owner{};

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot, bool OwnsRuntimeData,
          bool MergeParentCollections = false>
struct static_container_runtime_config {
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using parent_registry_type = ParentRegistry;
  using resolve_root_type = ResolveRoot;

  static constexpr bool owns_runtime_data = OwnsRuntimeData;
  static constexpr bool merge_parent_collections = MergeParentCollections;
};

template <typename BindingsState> struct runtime_scope_state {
  template <typename Allocator>
  runtime_scope_state(Allocator &allocator, void *parent)
      : bindings(allocator), registration_parent(parent) {}

  BindingsState bindings;
  void *registration_parent = nullptr;
};

template <typename Allocator, typename BindingsState>
struct runtime_registry_state {
  using scope_state_type = runtime_scope_state<BindingsState>;
  using scope_pointer_allocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<scope_state_type *>;

  explicit runtime_registry_state(Allocator &allocator)
      : runtime(allocator), bindings(allocator),
        scope_states(scope_pointer_allocator(allocator)) {}

  std::size_t create_scope(scope_state_type *state) {
    scope_states.push_back(state);
    return scope_states.size();
  }

  void release_scope(std::size_t scope, scope_state_type *state) noexcept {
    assert(scope > 0 && scope <= scope_states.size());
    assert(scope_states[scope - 1] == state);
    (void)state;
    scope_states[scope - 1] = nullptr;
    if (scope == scope_states.size()) {
      scope_states.pop_back();
    }
  }

  scope_state_type *find_scope(std::size_t scope) noexcept {
    assert(scope > 0 && scope <= scope_states.size());
    return scope_states[scope - 1];
  }

  runtime_storage<Allocator> runtime;
  BindingsState bindings;
  std::vector<scope_state_type *, scope_pointer_allocator> scope_states;
};

template <typename Data, bool OwnsData> class runtime_state_holder;

template <typename Data> class runtime_state_holder<Data, true> {
public:
  template <typename Allocator>
  explicit runtime_state_holder(Allocator &allocator) : data_(allocator) {}

  Data &get() noexcept { return data_; }

  static constexpr std::size_t scope_id() noexcept { return 0; }

private:
  Data data_;
};

template <typename Data> class runtime_state_holder<Data, false> {
public:
  static constexpr std::size_t invalid_scope =
      std::numeric_limits<std::size_t>::max();

  runtime_state_holder() = default;

  std::size_t scope_id() const noexcept { return scope_id_; }
  void scope_id(std::size_t value) noexcept { scope_id_ = value; }

private:
  std::size_t scope_id_ = invalid_scope;
};

template <typename Container, typename Request, typename LookupKey,
          typename = void>
struct has_key_value_lookup_definition : std::false_type {};

template <typename Container, typename Request, typename LookupKey>
struct has_key_value_lookup_definition<
    Container, Request, LookupKey,
    std::void_t<typename Container::lookup_definition_type>>
    : std::bool_constant<!std::is_void_v<selected_lookup_entry_t<
          normalized_type_t<Request>,
          typename std::decay_t<LookupKey>::definition_type,
          normalize_lookup_definitions_t<
              typename Container::lookup_definition_type>>>> {};

template <typename Container, typename Request, typename LookupKey>
inline constexpr bool has_key_value_lookup_definition_v =
    has_key_value_lookup_definition<Container, Request, LookupKey>::value;

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

template <typename Container, typename Request, typename LookupKey>
constexpr bool has_lookup() {
  static_assert(is_lookup_key_v<LookupKey>,
                "internal lookup queries require dingo::detail::lookup_key");
  using key_definition = typename std::decay_t<LookupKey>::definition_type;
  if constexpr (is_static_lookup_key_definition_v<key_definition>) {
    return true;
  } else {
    return has_key_value_lookup_definition_v<Container, Request, LookupKey>;
  }
}

// The part of a runtime registry that depends only on the container traits and
// the allocator: execution over a selected binding, registration commit and
// the rollback of both. Lookup selection lives in lookup_selection, which this
// store extends; runtime_registry derives from it, so the machinery is
// instantiated once per traits/allocator pair instead of once per registry
// specialization. Data ownership stays in runtime_registry, which hands the
// active lookup state to the store.
template <typename ContainerTraits, typename Allocator>
class registry_store : public registration_store<ContainerTraits, Allocator> {
  using lookup_base = lookup_selection<ContainerTraits, Allocator>;

protected:
  using store_type = registry_store;
  using runtime_transaction_type = runtime_transaction<Allocator>;
  using typename lookup_base::allocator_type;
  using typename lookup_base::base_lookup_cardinality;
  using typename lookup_base::base_lookup_definition;
  using typename lookup_base::base_lookup_entry;
  using typename lookup_base::lookup_definition_type;
  using typename lookup_base::lookup_index_entries;
  using typename lookup_base::normalized_lookup_entries;
  using typename lookup_base::rtti_type;
  using typename lookup_base::runtime_binding_interface_type;
  using typename lookup_base::runtime_bindings_state;
  using typename lookup_base::runtime_context_type;
  using typename lookup_base::runtime_lookup_binding_view;
  using typename lookup_base::runtime_lookup_value;
  using typename lookup_base::runtime_selection;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      typename lookup_base::template lookup_entry_cardinality<LookupEntry>;
  template <typename LookupEntry>
  using runtime_lookup_entry_traits =
      typename lookup_base::template runtime_lookup_entry_traits<LookupEntry>;
  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename lookup_base::template runtime_lookup_entries_for_interface_t<
          TypeInterface>;
  template <typename Interface, typename Key>
  using lookup_index_route =
      typename lookup_base::template lookup_index_route<Interface, Key>;
  template <typename Interface, typename KeyDomain, typename Cardinality>
  using static_key_lookup_entry_t =
      typename lookup_base::template static_key_lookup_entry_t<
          Interface, KeyDomain, Cardinality>;
  template <typename Interface, typename KeyDomain>
  using static_key_lookup_cardinality_t =
      typename lookup_base::template static_key_lookup_cardinality_t<Interface,
                                                                     KeyDomain>;
  using lookup_base::binding_status;
  using lookup_base::count;
  using lookup_base::for_each_collection_entry;
  using lookup_base::has_explicit_collection_lookup;
  using lookup_base::source_select;

public:
  using runtime_data_type =
      detail::runtime_registry_state<allocator_type, runtime_bindings_state>;

protected:
  using runtime_scope_state = typename runtime_data_type::scope_state_type;

  registry_store() { validate_lookup_definitions(); }

  registry_store(const registry_store &) = delete;
  registry_store &operator=(const registry_store &) = delete;

  ~registry_store() = default;

  static constexpr void validate_lookup_definitions() {
    using entries_type = normalized_lookup_entries;
    static_assert(
        !detail::has_duplicate_base_lookup_definition_v<lookup_definition_type>,
        "duplicate dingo base lookup definition");
    static_assert(!detail::has_duplicate_lookup_definition_v<entries_type>,
                  "duplicate dingo lookup definition for interface/key "
                  "domain/cardinality");
    static_assert(!detail::has_duplicate_lookup_key_type_v<entries_type>,
                  "conflicting dingo lookup definitions for interface/key "
                  "domain");
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(runtime_bindings_state *state,
                         construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    T results;
    const std::size_t total = this->template count<T>(state, key);
    if (total == 0 &&
        !(this->template has_explicit_collection_lookup<T>(key))) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    collection_type::reserve(results, total);
    collect(state, results, scope, context, std::forward<Fn>(fn),
            std::move(key));
    return results;
  }

  template <typename Request, typename Result = Request>
  Result resolve_named(runtime_selection selection, construction_scope scope,
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
  std::size_t collect(runtime_bindings_state *state, T &results,
                      construction_scope scope, runtime_context_type &context,
                      Fn &&fn, LookupKey key) {
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
};

} // namespace detail

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot, bool OwnsRuntimeData = true>
class DINGO_EMPTY_BASES runtime_registry
    : public allocator_base<Allocator>,
      public detail::runtime_registration<
          runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                           ResolveRoot, OwnsRuntimeData>>,
      protected detail::registry_store<ContainerTraits, Allocator> {
  template <typename> friend class runtime_context;
  template <typename> friend class detail::container_selection;
  template <typename> friend class detail::context_resolution;
  template <typename> friend class detail::runtime_registration;
  template <typename, typename, typename, typename>
  friend class runtime_binding_state;
  template <typename, typename, typename>
  friend class detail::runtime_container_impl;
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
  using runtime_type = runtime_storage<Allocator>;
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
      detail::runtime_container_impl<Bindings, Parent,
                                     registration_runtime_config<Parent>>>;

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

public:
  runtime_registry(const runtime_registry &) = delete;
  runtime_registry &operator=(const runtime_registry &) = delete;

  ~runtime_registry() = default;

  allocator_type &get_allocator() {
    return allocator_base<allocator_type>::get_allocator();
  }

protected:
  runtime_storage<allocator_type> &runtime() {
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
  std::size_t count(LookupKey key) {
    return store_type::template count<T>(runtime_bindings(), std::move(key));
  }

  template <typename T, typename Fn>
  std::size_t collect(construction_scope scope, T &results,
                      runtime_context_type &context, Fn &&fn) {
    return store_type::collect(runtime_bindings(), results, scope, context,
                               std::forward<Fn>(fn), detail::no_lookup_key());
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t collect(construction_scope scope, T &results,
                      runtime_context_type &context, Fn &&fn, LookupKey key) {
    return store_type::collect(runtime_bindings(), results, scope, context,
                               std::forward<Fn>(fn), std::move(key));
  }

  template <typename Value, typename Exact, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(LookupKey key) {
    return store_type::template binding_status<Value, Exact, lookup_may_miss()>(
        runtime_bindings(), std::move(key));
  }
  template <typename Source> class container_proxy {
  public:
    using container_type = typename Source::container_type;

    explicit container_proxy(Source *source) : source_(source) {}

    container_type &get() const { return source_->get_container(); }
    container_type *operator->() const { return std::addressof(get()); }
    operator container_type &() const { return get(); }

  private:
    Source *source_;
  };

protected:
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R source_missing(construction_scope scope, runtime_context_type &context,
                   const LookupKey &key) {
    using Type = typename Request::value_type;
    using user_type = typename Request::user_type;
    using lookup_key_type = std::decay_t<LookupKey>;

    if constexpr (MayAutoConstruct && collection_traits<R>::is_collection) {
      if (this->template count<R>(key) != 0) {
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
    } else if constexpr (MayAutoConstruct &&
                         is_auto_constructible<
                             std::decay_t<user_type>>::value) {
      if constexpr (constructor<Type>::kind ==
                    detail::constructor_kind::concrete) {
        return auto_construct<Request>(scope, context);
      } else {
        throw detail::make_type_not_found_exception<
            typename Request::lookup_type>(context, key);
      }
    } else {
      throw detail::make_type_not_found_exception<
          typename Request::lookup_type>(context, key);
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
  template <typename... TypeArgs, typename Parent, typename Arg,
            typename... KeyValueArgs>
  // Registration is cold; keep its type-specific setup out of callers.
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_NOINLINE auto prepare_binding(Parent *parent, Arg &&arg,
                                      KeyValueArgs &&...key_values) {
    static_assert(!detail::has_explicit_void_interface_v<TypeArgs...>,
                  "interfaces<void> is not a valid registration target");
    using registration =
        std::conditional_t<!is_none_v<std::decay_t<Arg>>,
                           type_registration<TypeArgs..., factory<Arg>>,
                           type_registration<TypeArgs...>>;
    static_assert(
        !detail::is_key_value_v<typename registration::key_type>,
        "dingo::key_type<T, V> registration keys require a static fixed "
        "runtime-key request");
    using binding_model = detail::binding_model<registration>;
    using bindings_type = typename binding_model::bindings_type;
    using instance_container_type =
        registration_container_type<bindings_type, Parent>;
    (void)std::addressof(arg);
    using interface_types = typename binding_model::interface_types;
    static constexpr bool storage_tag_is_complete =
        binding_model::storage_tag_is_complete;
    static_assert(storage_tag_is_complete,
                  "registered storage tag must be complete; include the "
                  "corresponding dingo/storage header");
    if constexpr (storage_tag_is_complete) {
      using storage_type = typename binding_model::storage_type;
      using registration_requirements = typename binding_model::requirements;
      using registration_key = typename binding_model::key_type;

      registration_requirements::assert_valid();

      using runtime_binding_state_type =
          detail::runtime_binding_state_t<registry_type,
                                          instance_container_type, storage_type,
                                          bindings_type, Parent>;

      if constexpr (registration_requirements::valid &&
                    type_list_size_v<interface_types> == 1) {
        if constexpr (sizeof...(KeyValueArgs) > 0) {
          store_type::template validate_supplied_runtime_registration_keys<
              interface_types, KeyValueArgs...>();
        }
        using interface_type = type_list_head_t<interface_types>;

        using owner_type =
            runtime_binding_value_owner_t<interface_types, storage_type,
                                          runtime_binding_state_type,
                                          registration_key>;
        static constexpr bool shared_owner =
            sizeof...(KeyValueArgs) > 0 &&
            type_list_size_v<
                runtime_lookup_entries_for_interface_t<interface_type>> > 1;

        if constexpr (!is_none_v<std::decay_t<Arg>>) {
          return commit_binding<shared_owner, interface_type, storage_type,
                                owner_type>(
              parent, registration_key{},
              std::forward_as_tuple(std::forward<KeyValueArgs>(key_values)...),
              std::forward<Arg>(arg));
        } else {
          return commit_binding<shared_owner, interface_type, storage_type,
                                owner_type>(
              parent, registration_key{},
              std::forward_as_tuple(std::forward<KeyValueArgs>(key_values)...));
        }
      } else {
        if constexpr (registration_requirements::valid) {
          if constexpr (sizeof...(KeyValueArgs) > 0) {
            store_type::template validate_supplied_runtime_registration_keys<
                interface_types, KeyValueArgs...>();
          }
          inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
          runtime_transaction_type transaction(runtime(), scratch);
          auto &state = ensure_runtime_bindings(transaction, parent);
          runtime_binding_state_type *data = nullptr;
          if constexpr (!is_none_v<std::decay_t<Arg>>) {
            data =
                transaction
                    .template construct_persistent<runtime_binding_state_type>(
                        this, std::forward<Arg>(arg));
          } else {
            data =
                transaction
                    .template construct_persistent<runtime_binding_state_type>(
                        this);
          }

          using owner_type =
              runtime_binding_value_owner_t<interface_types, storage_type,
                                            runtime_binding_state_type *,
                                            registration_key>;
          auto &binding_owner =
              *transaction.template construct_persistent<owner_type>(data);
          auto *binding_result = std::addressof(binding_owner);
          shared_lookup_value_factory<owner_type> value_factory{
              std::addressof(binding_owner)};

          for_each(interface_types{}, [&](auto element) {
            using interface_type = typename decltype(element)::type;
            this->template commit_registration<interface_type, storage_type>(
                state, value_factory, transaction, registration_key{},
                key_values...);
          });
          transaction.commit();
          return container_proxy<owner_type>(binding_result);
        } else {
          return store_type::template invalid_registration_return<
              instance_container_type>();
        }
      }
    } else {
      return store_type::template invalid_registration_return<
          instance_container_type>();
    }
  }

  template <bool SharedOwner, typename TypeInterface, typename TypeStorage,
            typename Owner, typename Parent, typename LookupKey,
            typename KeyValueTuple, typename... Args>
  DINGO_NOINLINE container_proxy<Owner>
  commit_binding(Parent *parent, LookupKey lookup_key,
                 KeyValueTuple &&key_values, Args &&...args) {
    inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
    runtime_transaction_type transaction(runtime(), scratch);
    auto &state = ensure_runtime_bindings(transaction, parent);
    auto &binding_owner = *transaction.template construct_persistent<Owner>(
        this, std::forward<Args>(args)...);
    auto *binding_result = std::addressof(binding_owner);
    using route = lookup_index_route<TypeInterface, LookupKey>;
    constexpr bool uses_singular_base_lookup =
        std::is_same_v<typename route::entry, base_lookup_entry> &&
        std::is_same_v<
            typename lookup_entry_cardinality<base_lookup_entry>::type,
            ::dingo::one>;
    // Most registrations publish one interface through the base lookup. Keep
    // that path direct so each registered type does not instantiate the
    // runtime-key factory and recursive lookup machinery.
    if constexpr (!SharedOwner &&
                  std::tuple_size_v<std::remove_reference_t<KeyValueTuple>> ==
                      0 &&
                  uses_singular_base_lookup) {
      this->template check_interface_requirements<
          TypeStorage, typename annotated_traits<TypeInterface>::type,
          typename TypeStorage::type>();
      auto key_arg = route::key(lookup_key);
      if (!this->commit_singular_base_lookup(
              state, transaction, key_arg,
              runtime_lookup_value(runtime_lookup_binding_view{
                  static_cast<runtime_binding_interface_type *>(
                      binding_result)}))) {
        throw detail::make_lookup_already_registered_exception<
            TypeInterface, typename TypeStorage::type>(lookup_key, key_arg);
      }
    } else {
      direct_lookup_value_factory<SharedOwner, Owner> value_factory{
          std::addressof(binding_owner)};
      std::apply(
          [&](auto &&...key_value_args) {
            this->template commit_registration<TypeInterface, TypeStorage>(
                state, value_factory, transaction, lookup_key,
                std::forward<decltype(key_value_args)>(key_value_args)...);
          },
          std::forward<KeyValueTuple>(key_values));
    }
    transaction.commit();
    return container_proxy<Owner>(binding_result);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename Request>
  decltype(auto) auto_construct(construction_scope scope,
                                runtime_context_type &context) {
    using Type = typename Request::value_type;

    static_assert(is_complete<Type>::value,
                  "auto-construction requires a complete type");

    using type_detection = detail::constructor_shape;
    return context
        .template construct<typename Request::interface_type, type_detection>(
            scope, *resolve_root());
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    using interface_type = typename request::interface_type;
    auto selected =
        this->template select_binding<typename request::value_type,
                                      typename request::exact_type>(key);
    if (selected.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename request::lookup_type>(context);
    }
    if (selected.status == detail::binding_status::found) {
      return this->template resolve_named<typename request::lookup_type,
                                          interface_type>(selected, scope,
                                                          context);
    }
    return this->template source_missing<
        request,
        detail::is_runtime_auto_constructible_dependency_v<
            typename request::user_type>,
        LookupKey, interface_type>(scope, context, std::move(key));
  }

  parent_container_type *parent_ = nullptr;

  detail::runtime_state_holder<runtime_data_type, OwnsRuntimeData>
      runtime_data_;
};

} // namespace dingo
