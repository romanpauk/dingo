//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/auto_constructible.h>
#include <dingo/core/binding_model.h>
#include <dingo/core/binding_resolution_policy.h>
#include <dingo/core/binding_selection.h>
#include <dingo/core/exceptions.h>
#include <dingo/core/key.h>
#include <dingo/core/none.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/lookup.h>
#include <dingo/lookup/operations.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/registration/requirements.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/runtime/container_runtime.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/runtime/transaction.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/type_list.h>

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

struct runtime_data_owner_t {};
inline constexpr runtime_data_owner_t runtime_data_owner{};

template <typename RootTraits, typename Tag>
struct registration_container_traits {
private:
  using rebound_traits = typename RootTraits::template rebind_t<Tag>;
  static constexpr bool rebinds_tag =
      !std::is_same_v<typename rebound_traits::tag_type,
                      typename RootTraits::tag_type>;

public:
  template <typename ReboundTag>
  using rebind_t =
      std::conditional_t<rebinds_tag,
                         registration_container_traits<RootTraits, ReboundTag>,
                         registration_container_traits<RootTraits, Tag>>;

  using tag_type =
      std::conditional_t<rebinds_tag, typename rebound_traits::tag_type,
                         typename RootTraits::tag_type>;
  using rtti_type = typename RootTraits::rtti_type;
  using allocator_type = typename RootTraits::allocator_type;
  using lookup_definition_type = container_lookup_definition_type_t<RootTraits>;
  static constexpr bool dependency_observation_enabled =
      dependency_observation_v<RootTraits>;
};

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

template <typename BindingInterface> struct runtime_lookup_binding_view {
  BindingInterface *binding = nullptr;
};

template <typename BindingInterface> class runtime_lookup_value {
public:
  using binding_view_type = runtime_lookup_binding_view<BindingInterface>;

  explicit runtime_lookup_value(binding_view_type view) : view_(view) {}

  runtime_lookup_value(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value &operator=(runtime_lookup_value &&) noexcept = default;
  runtime_lookup_value(const runtime_lookup_value &) = delete;
  runtime_lookup_value &operator=(const runtime_lookup_value &) = delete;

  BindingInterface &binding() { return *view_.binding; }
  const BindingInterface &binding() const { return *view_.binding; }

private:
  binding_view_type view_;
};

template <typename LookupEntries, typename BindingInterface, typename Allocator>
struct runtime_bindings_state {
  using value_type = runtime_lookup_value<BindingInterface>;

  explicit runtime_bindings_state(Allocator &allocator)
      : lookup_indexes(allocator) {}

  lookup_index<LookupEntries, value_type, Allocator> lookup_indexes;
};

template <typename BindingsState> struct runtime_scope_state {
  template <typename Allocator>
  runtime_scope_state(Allocator &allocator, void *parent)
      : bindings(allocator), registration_parent(parent) {}

  BindingsState bindings;
  void *registration_parent = nullptr;
};

template <typename Allocator, typename BindingsState>
struct runtime_registry_data {
  using scope_state_type = runtime_scope_state<BindingsState>;
  using scope_pointer_allocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<scope_state_type *>;

  explicit runtime_registry_data(Allocator &allocator)
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

  container_runtime<Allocator> runtime;
  BindingsState bindings;
  std::vector<scope_state_type *, scope_pointer_allocator> scope_states;
};

template <typename Data, bool OwnsData> class runtime_data_holder;

template <typename Data> class runtime_data_holder<Data, true> {
public:
  template <typename Allocator>
  explicit runtime_data_holder(Allocator &allocator) : data_(allocator) {}

  Data &get() noexcept { return data_; }

  static constexpr std::size_t scope_id() noexcept { return 0; }

private:
  Data data_;
};

template <typename Data> class runtime_data_holder<Data, false> {
public:
  static constexpr std::size_t invalid_scope =
      std::numeric_limits<std::size_t>::max();

  runtime_data_holder() = default;

  std::size_t scope_id() const noexcept { return scope_id_; }
  void scope_id(std::size_t value) noexcept { scope_id_ = value; }

private:
  std::size_t scope_id_ = invalid_scope;
};

template <typename Interface> struct key_value_interface {
  using type = normalized_type_t<Interface>;
};

template <> struct key_value_interface<void> {
  using type = void;
};

template <typename Interface>
using key_value_interface_t = typename key_value_interface<Interface>::type;

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

template <typename T>
inline constexpr bool is_runtime_auto_constructible_dependency_v =
    std::is_same_v<typename request_type<T>::value_type, std::decay_t<T>> &&
    (!std::is_reference_v<T> ||
     (std::is_lvalue_reference_v<T> &&
      std::is_const_v<std::remove_reference_t<T>> &&
      is_auto_constructible<std::decay_t<T>>::value));

template <typename T, typename = void>
struct has_static_registry_type : std::false_type {};

template <typename T>
struct has_static_registry_type<T,
                                std::void_t<typename T::static_registry_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_static_registry_type_v =
    has_static_registry_type<T>::value;

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

} // namespace detail

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot, bool OwnsRuntimeData = true>
class runtime_registry : public allocator_base<Allocator> {
  template <typename> friend class runtime_context;
  template <typename> friend class detail::runtime_registration_api;
  template <typename, typename, typename, typename, typename>
  friend class runtime_binding_state;
  template <typename, typename> friend class detail::binding_resolution;
  template <typename, typename, typename>
  friend class detail::container_with_static_bindings;
  template <typename, typename, typename> friend class runtime_container;
  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT,
            bool OwnsRuntimeDataT>
  friend class runtime_registry;
  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT,
            bool OwnsRuntimeDataT>
  using rebind_t =
      runtime_registry<ContainerTraitsT, AllocatorT, ParentRegistryT,
                       ResolveRootT, OwnsRuntimeDataT>;
  using registry_type =
      runtime_registry<ContainerTraits, Allocator, ParentRegistry, ResolveRoot,
                       OwnsRuntimeData>;
  using resolve_root_type =
      std::conditional_t<std::is_same_v<void, ResolveRoot>, registry_type,
                         ResolveRoot>;
  using container_type = resolve_root_type;
  using runtime_type = container_runtime<Allocator>;
  using runtime_context_type = runtime_context<Allocator>;
  using runtime_transaction_type = runtime_transaction<Allocator>;
  using runtime_binding_interface_type =
      runtime_binding_interface<container_type, runtime_context_type>;
  using runtime_selection =
      detail::runtime_binding_selection<runtime_binding_interface_type>;
  template <typename Registration>
  using registration_container_traits_type =
      detail::registration_container_traits<
          ContainerTraits, type_list<typename ContainerTraits::tag_type,
                                     typename Registration::interface_type>>;
  template <typename Registration, typename Parent>
  using registration_runtime_config = detail::static_container_runtime_config<
      registration_container_traits_type<Registration>, Allocator, Parent,
      resolve_root_type, false, true>;
  template <typename Registration, typename Parent>
  using runtime_registration_container_type =
      runtime_registry<registration_container_traits_type<Registration>,
                       Allocator, Parent, resolve_root_type, false>;
  template <typename Registration, typename Bindings, typename Parent>
  using registration_container_type = std::conditional_t<
      std::is_void_v<Bindings>,
      runtime_registration_container_type<Registration, Parent>,
      detail::container_with_static_bindings<
          Bindings, Parent, registration_runtime_config<Registration, Parent>>>;
  using parent_registry_type =
      std::conditional_t<std::is_same_v<void, ParentRegistry>, registry_type,
                         ParentRegistry>;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using parent_container_type =
      std::conditional_t<std::is_same_v<void, ParentRegistry>,
                         resolve_root_type, ParentRegistry>;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;
  static constexpr bool dependency_observation_enabled =
      detail::dependency_observation_v<container_traits_type>;

protected:
  using normalized_lookup_entries =
      detail::normalize_lookup_definitions_t<lookup_definition_type>;
  using base_lookup_definition =
      detail::base_lookup_definition_t<lookup_definition_type>;
  using base_lookup_entry =
      detail::base_lookup_entry<rtti_type,
                                typename base_lookup_definition::cardinality,
                                typename base_lookup_definition::backend_type>;
  using base_lookup_cardinality = typename base_lookup_definition::cardinality;
  using lookup_index_entries = type_list_unique_t<
      type_list_cat_t<normalized_lookup_entries, type_list<base_lookup_entry>>>;
  template <typename LookupEntry>
  using lookup_entry_cardinality =
      detail::lookup_entry_cardinality<LookupEntry>;

  template <typename LookupEntry>
  static constexpr bool lookup_entry_indexed_v =
      detail::contains_lookup_definition<LookupEntry,
                                         lookup_index_entries>::value;

  using runtime_lookup_binding_view =
      detail::runtime_lookup_binding_view<runtime_binding_interface_type>;
  using runtime_lookup_value =
      detail::runtime_lookup_value<runtime_binding_interface_type>;
  using runtime_bindings_state = detail::runtime_bindings_state<
      lookup_index_entries, runtime_binding_interface_type, allocator_type>;
  using runtime_data_type =
      detail::runtime_registry_data<allocator_type, runtime_bindings_state>;
  using runtime_scope_state = typename runtime_data_type::scope_state_type;

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
  runtime_registry()
      : allocator_base<allocator_type>(allocator_type()),
        runtime_data_(get_allocator()) {
    static_assert(OwnsRuntimeData);
    validate_lookup_definitions();
  }

  runtime_registry(const allocator_type &alloc)
      : allocator_base<allocator_type>(alloc), runtime_data_(get_allocator()) {
    static_assert(OwnsRuntimeData);
    validate_lookup_definitions();
  }

  runtime_registry(detail::runtime_data_owner_t, parent_container_type *parent,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), parent_(parent),
        runtime_data_(get_allocator()) {
    static_assert(OwnsRuntimeData);
    validate_lookup_definitions();
    validate_parent_registry();
  }

  template <bool Enabled = OwnsRuntimeData, std::enable_if_t<!Enabled, int> = 0>
  runtime_registry(parent_container_type *parent,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), parent_(parent),
        runtime_data_() {
    validate_lookup_definitions();
    validate_parent_registry();
  }

private:
  registry_type &binding_store() { return *this; }

  static constexpr void validate_parent_registry() {
    if constexpr (!std::is_same_v<void, ParentRegistry>) {
      static_assert(
          !detail::is_tagged_container_v<container_traits_type> ||
              !std::is_same_v<typename container_traits_type::tag_type,
                              typename parent_registry_type::
                                  container_traits_type::tag_type>,
          "static typemap based containers require parent and child "
          "container tags to be different");
    }
  }

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

public:
  runtime_registry(const runtime_registry &) = delete;
  runtime_registry &operator=(const runtime_registry &) = delete;

  ~runtime_registry() = default;

  template <typename Visitor> bool visit_registrations(Visitor &&visitor) {
    auto *state = runtime_bindings();
    if (state == nullptr) {
      return true;
    }

    bool complete = true;
    const std::size_t container_id = runtime_data_.scope_id();
    for_each(lookup_index_entries{}, [&](auto element) {
      using entry_type = typename decltype(element)::type;
      auto &backend = state->lookup_indexes.template get<entry_type>();
      const bool inspected = backend.for_each([&](const auto &key,
                                                  const auto &value) {
        auto view = value.binding().introspection(container_id);
        if constexpr (!std::is_same_v<entry_type, base_lookup_entry>) {
          view.key_type =
              describe_type<typename entry_type::key_domain::definition_type>();
          if constexpr (!std::is_same_v<std::decay_t<decltype(key)>, none_t>) {
            view.key = introspection_key::from(key);
          }
        }
        visitor(view);
      });
      complete = inspected && complete;
    });
    return complete;
  }

  template <typename Observer>
  dependency_observer_subscription
  observe_dependencies(Observer &observer) noexcept {
    static_assert(dependency_observation_enabled,
                  "dependency observation requires traits with "
                  "dependency_observation_enabled = true");
    return runtime().observe_dependencies(observer);
  }

  std::size_t container_id() const noexcept { return runtime_data_.scope_id(); }

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
      const auto scope = data.create_scope(state);
      runtime_data_.scope_id(scope);
      try {
        transaction.on_rollback([this, &data, scope, state]() noexcept {
          data.release_scope(scope, state);
          runtime_data_.scope_id(decltype(runtime_data_)::invalid_scope);
        });
      } catch (...) {
        data.release_scope(scope, state);
        runtime_data_.scope_id(decltype(runtime_data_)::invalid_scope);
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
    if constexpr (std::is_same_v<void, ResolveRoot> ||
                  std::is_same_v<resolve_root_type, registry_type>) {
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

public:
  template <typename... TypeArgs> auto register_type() {
    return prepare_binding<TypeArgs...>(this, none_t{});
  }

  template <typename... TypeArgs, typename Arg,
            std::enable_if_t<!detail::is_runtime_registration_key_arg_v<Arg>,
                             int> = 0>
  auto register_type(Arg &&arg) {
    return prepare_binding<TypeArgs...>(this, std::forward<Arg>(arg));
  }

  template <typename... TypeArgs, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 detail::are_runtime_registration_key_args_v<KeyArgs...>),
                int> = 0>
  auto register_type(KeyArgs &&...keys) {
    return prepare_binding<TypeArgs...>(this, none_t{},
                                        std::forward<KeyArgs>(keys)...);
  }

  template <typename... TypeArgs, typename Arg, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 !detail::is_runtime_registration_key_arg_v<Arg> &&
                 detail::are_runtime_registration_key_args_v<KeyArgs...>),
                int> = 0>
  auto register_type(Arg &&arg, KeyArgs &&...keys) {
    return prepare_binding<TypeArgs...>(this, std::forward<Arg>(arg),
                                        std::forward<KeyArgs>(keys)...);
  }

protected:
  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    T results;
    const std::size_t count = count_collection<T>(key);
    if (count == 0 && !has_explicit_collection_lookup<T>(key)) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    collection_type::reserve(results, count);
    append_runtime_collection(results, scope, context, std::forward<Fn>(fn),
                              std::move(key));
    return results;
  }

  template <typename Signature = void, typename Callable>
  auto invoke(runtime_context_type &context, Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        detail::callable_dispatch_signature_t<Signature, callable_type>;

    auto type_guard = context.template track_type<callable_type>();
    return detail::callable_invoke<dispatch_signature>::construct(
        std::forward<Callable>(callable), ephemeral_scope, context,
        *resolve_root());
  }

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  runtime_selection select_binding(LookupKey key) {
    return source_select<Request>(std::move(key));
  }

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(LookupKey key) {
    auto *state = runtime_bindings();
    if (!state) {
      return detail::binding_status::not_found;
    }
    auto selection = select_binding<Request>(*state, key);
    return selection.status;
  }

  template <typename T, typename Fn>
  std::size_t append_collection(T &results, construction_scope scope,
                                runtime_context_type &context, Fn &&fn) {
    return append_runtime_collection(
        results, scope, context, std::forward<Fn>(fn), detail::no_lookup_key());
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(T &results, construction_scope scope,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return append_runtime_collection(results, scope, context,
                                     std::forward<Fn>(fn), std::move(key));
  }

  template <typename Request, typename Result = Request>
  Result resolve_binding(runtime_selection selection, construction_scope scope,
                         runtime_context_type &context) {
    auto &binding = *selection.binding;
    if constexpr (detail::cache::supports_v<Request>) {
      cache_update update{binding.cache_slot(), std::addressof(context),
                          detail::cache::key<Request>()};
      return resolve<Request, Result>(scope, binding, context, update.sink());
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

  template <typename Source> class container_proxy {
  public:
    using container_type = typename Source::container_type;

    explicit container_proxy(Source *source) : source_(source) {}

    container_type &get() const { return source_->container(); }
    container_type *operator->() const { return std::addressof(get()); }
    operator container_type &() const { return get(); }

  private:
    Source *source_;
  };

  template <typename T> static T invalid_registration_return();

  struct runtime_binding_value {
    runtime_binding_value(const runtime_binding_value &) = delete;
    runtime_binding_value &operator=(const runtime_binding_value &) = delete;
    runtime_binding_value(runtime_binding_value &&) = delete;
    runtime_binding_value &operator=(runtime_binding_value &&) = delete;

  protected:
    runtime_binding_value() = default;
    ~runtime_binding_value() = default;
  };

  template <std::size_t Index, typename Binding>
  struct runtime_binding_member : Binding {
    template <typename... Args>
    explicit runtime_binding_member(Args &&...args)
        : Binding(std::forward<Args>(args)...) {}

    typename Binding::container_type &container() {
      return this->get_container();
    }
  };

  template <typename Derived, typename Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base;

  template <typename Derived, std::size_t... Indexes, typename... Bindings>
  struct runtime_binding_value_impl_base<
      Derived, std::index_sequence<Indexes...>, Bindings...>
      : runtime_binding_value, runtime_binding_member<Indexes, Bindings>... {
    using container_type =
        typename type_list_head_t<type_list<Bindings...>>::container_type;

    // NOLINTBEGIN(bugprone-use-after-move)
    template <typename... Args>
    explicit runtime_binding_value_impl_base(Args &&...args)
        : runtime_binding_member<Indexes, Bindings>(
              std::forward<Args>(args)...)... {}
    // NOLINTEND(bugprone-use-after-move)

    template <typename Interface> runtime_lookup_binding_view view() {
      return view_impl<Interface, 0, Bindings...>();
    }

    container_type &container() {
      return static_cast<
                 typename registry_type::template runtime_binding_member<
                     0, type_list_head_t<type_list<Bindings...>>> &>(*this)
          .container();
    }

  private:
    template <typename Interface, std::size_t Index, typename Head,
              typename... Tail>
    runtime_lookup_binding_view view_impl() {
      if constexpr (std::is_same_v<
                        typename Head::interface_type,
                        typename annotated_traits<Interface>::type>) {
        auto &binding =
            static_cast<typename registry_type::template runtime_binding_member<
                Index, Head> &>(*this);
        return {std::addressof(binding)};
      } else if constexpr (sizeof...(Tail) > 0) {
        return view_impl<Interface, Index + 1, Tail...>();
      } else {
        static_assert(sizeof...(Tail) > 0,
                      "lookup interface is not present in runtime binding "
                      "value");
      }
    }
  };

  template <typename... Bindings>
  struct runtime_binding_value_impl final
      : runtime_binding_value_impl_base<runtime_binding_value_impl<Bindings...>,
                                        std::index_sequence_for<Bindings...>,
                                        Bindings...> {
    using base_type =
        runtime_binding_value_impl_base<runtime_binding_value_impl<Bindings...>,
                                        std::index_sequence_for<Bindings...>,
                                        Bindings...>;

    using base_type::base_type;
  };

protected:
  template <typename LookupKeyDefinition> struct base_lookup_key_type;

  template <typename T>
  struct base_lookup_key_type<detail::lookup_key<key_type<T>>> {
    using type = T;
  };

  template <typename T, typename Interface>
  struct base_lookup_key_type<detail::lookup_key<key_value<T, Interface>>> {
    using type = T;
  };

  template <typename Interface, typename Key>
  static detail::base_lookup_key<rtti_type> base_typed_key_lookup_key() {
    using key_definition = typename std::decay_t<Key>::definition_type;
    using key_type = typename base_lookup_key_type<key_definition>::type;
    return {rtti_type::template get_type_index<Interface>(),
            rtti_type::template get_type_index<key_type>()};
  }

  template <typename Interface, typename KeyDomain, typename Cardinality>
  using static_key_lookup_entry_t =
      detail::selected_lookup_entry_with_cardinality_t<
          Interface, KeyDomain, Cardinality, normalized_lookup_entries>;

  template <typename Interface, typename KeyDomain>
  using static_key_lookup_cardinality_t = std::conditional_t<
      std::is_void_v<
          static_key_lookup_entry_t<Interface, KeyDomain, ::dingo::one>> &&
          !std::is_void_v<
              static_key_lookup_entry_t<Interface, KeyDomain, ::dingo::many>>,
      ::dingo::many,
      std::conditional_t<!std::is_void_v<static_key_lookup_entry_t<
                             Interface, KeyDomain, ::dingo::one>>,
                         ::dingo::one, base_lookup_cardinality>>;

  template <typename LookupEntry> struct runtime_lookup_entry_traits {
    static constexpr bool is_key_value = false;
    using interface_type = void;
    using key_type = ::dingo::key_type<::dingo::none_t>;
  };

  template <typename Interface, typename Key, typename Cardinality,
            typename Backend>
  struct runtime_lookup_entry_traits<detail::lookup_entry<
      Interface, detail::lookup_key<key_value<Key>>, Cardinality, Backend>> {
    static constexpr bool is_key_value = true;
    using interface_type = Interface;
    using key_type = Key;
  };

  template <typename TypeInterface, typename Entries>
  struct runtime_lookup_entries_for_interface;

  template <typename TypeInterface>
  struct runtime_lookup_entries_for_interface<TypeInterface, type_list<>> {
    using type = type_list<>;
  };

  template <typename TypeInterface, typename Head, typename... Tail>
  struct runtime_lookup_entries_for_interface<TypeInterface,
                                              type_list<Head, Tail...>> {
  private:
    using head_traits = runtime_lookup_entry_traits<Head>;
    using tail_type =
        typename runtime_lookup_entries_for_interface<TypeInterface,
                                                      type_list<Tail...>>::type;
    static constexpr bool matches =
        head_traits::is_key_value &&
        std::is_same_v<normalized_type_t<TypeInterface>,
                       typename head_traits::interface_type>;

  public:
    using type =
        std::conditional_t<matches, type_list_cat_t<type_list<Head>, tail_type>,
                           tail_type>;
  };

  template <typename TypeInterface>
  using runtime_lookup_entries_for_interface_t =
      typename runtime_lookup_entries_for_interface<
          TypeInterface, normalized_lookup_entries>::type;

  template <typename Arg>
  using runtime_registration_arg_traits =
      detail::runtime_registration_key_arg_traits<std::decay_t<Arg>>;

  template <typename Entry, typename Arg>
  struct runtime_registration_arg_matches_entry {
  private:
    using entry_traits = runtime_lookup_entry_traits<Entry>;
    using arg_traits = runtime_registration_arg_traits<Arg>;
    using arg_interface =
        detail::key_value_interface_t<typename arg_traits::interface_type>;

  public:
    static constexpr bool value =
        std::is_same_v<typename entry_traits::key_type,
                       typename arg_traits::key_type> &&
        (std::is_void_v<arg_interface> ||
         std::is_same_v<typename entry_traits::interface_type, arg_interface>);
  };

  template <typename Entry, typename Arg>
  struct runtime_registration_arg_matches_qualified_entry {
  private:
    using entry_traits = runtime_lookup_entry_traits<Entry>;
    using arg_traits = runtime_registration_arg_traits<Arg>;
    using arg_interface =
        detail::key_value_interface_t<typename arg_traits::interface_type>;

  public:
    static constexpr bool value =
        !std::is_void_v<arg_interface> &&
        std::is_same_v<typename entry_traits::key_type,
                       typename arg_traits::key_type> &&
        std::is_same_v<typename entry_traits::interface_type, arg_interface>;
  };

  template <typename Left, typename Right>
  struct runtime_registration_args_are_same_key {
  private:
    using left_traits = runtime_registration_arg_traits<Left>;
    using right_traits = runtime_registration_arg_traits<Right>;
    using left_interface =
        detail::key_value_interface_t<typename left_traits::interface_type>;
    using right_interface =
        detail::key_value_interface_t<typename right_traits::interface_type>;

  public:
    static constexpr bool value =
        std::is_same_v<typename left_traits::key_type,
                       typename right_traits::key_type> &&
        std::is_same_v<left_interface, right_interface>;
  };

  template <typename Arg, typename... KeyValueArgs>
  static constexpr std::size_t runtime_registration_key_arg_count_v =
      (std::size_t{0} + ... +
       (runtime_registration_args_are_same_key<Arg, KeyValueArgs>::value ? 1U
                                                                         : 0U));

  template <typename Key, typename... KeyValueArgs>
  static constexpr std::size_t
      unqualified_runtime_registration_key_arg_count_v =
          (std::size_t{0} + ... +
           ((std::is_same_v<typename runtime_registration_arg_traits<
                                KeyValueArgs>::key_type,
                            Key> &&
             std::is_void_v<typename runtime_registration_arg_traits<
                 KeyValueArgs>::interface_type>)
                ? 1U
                : 0U));

  template <typename Entry, typename... KeyValueArgs>
  static constexpr std::size_t qualified_runtime_registration_key_arg_count_v =
      (std::size_t{0} + ... +
       (runtime_registration_arg_matches_qualified_entry<Entry,
                                                         KeyValueArgs>::value
            ? 1U
            : 0U));

  template <typename Entries, typename Key>
  struct runtime_lookup_entry_key_count;

  template <typename Key>
  struct runtime_lookup_entry_key_count<type_list<>, Key>
      : std::integral_constant<std::size_t, 0> {};

  template <typename Head, typename... Tail, typename Key>
  struct runtime_lookup_entry_key_count<type_list<Head, Tail...>, Key>
      : std::integral_constant<
            std::size_t,
            (runtime_registration_arg_matches_entry<Head, Key>::value ? 1U
                                                                      : 0U) +
                runtime_lookup_entry_key_count<type_list<Tail...>,
                                               Key>::value> {};

  template <typename InterfaceList, typename Arg>
  struct runtime_registration_key_match_count;

  template <typename Arg>
  struct runtime_registration_key_match_count<type_list<>, Arg>
      : std::integral_constant<std::size_t, 0> {};

  template <typename Interface, typename... Tail, typename Arg>
  struct runtime_registration_key_match_count<type_list<Interface, Tail...>,
                                              Arg>
      : std::integral_constant<
            std::size_t,
            runtime_lookup_entry_key_count<
                runtime_lookup_entries_for_interface_t<Interface>, Arg>::value +
                runtime_registration_key_match_count<type_list<Tail...>,
                                                     Arg>::value> {};

  template <typename InterfaceList, typename... KeyValueArgs>
  static constexpr void validate_supplied_runtime_registration_keys() {
    static_assert(
        ((runtime_registration_key_arg_count_v<KeyValueArgs, KeyValueArgs...> ==
          1U) &&
         ...),
        "duplicate runtime key value arguments for runtime lookup "
        "registration");
    static_assert(
        ((runtime_registration_key_match_count<InterfaceList,
                                               KeyValueArgs>::value > 0U) &&
         ...),
        "supplied runtime key value has no matching runtime-key lookup "
        "definition for the registered interface");
  }

  template <typename Entries, typename... KeyValueArgs>
  static constexpr void validate_required_runtime_registration_keys() {
    static_assert(type_list_size_v<Entries> > 0,
                  "runtime dingo::key_value<K>{value} registration requires a "
                  "matching runtime-key lookup definition");
    for_each(Entries{}, [](auto element) {
      using entry_type = typename decltype(element)::type;
      using key_type =
          typename runtime_lookup_entry_traits<entry_type>::key_type;
      static_assert(
          qualified_runtime_registration_key_arg_count_v<entry_type,
                                                         KeyValueArgs...> +
                  unqualified_runtime_registration_key_arg_count_v<
                      key_type, KeyValueArgs...> >=
              1U,
          "runtime-key lookup registration requires a runtime key value for "
          "each declared runtime-key lookup");
    });
  }

  template <typename Interface, typename KeyDefinition, typename = void>
  struct lookup_index_route_impl;

  template <typename Interface, typename KeyDefinition>
  struct lookup_index_route_impl<
      Interface, KeyDefinition,
      std::enable_if_t<
          detail::is_static_lookup_key_definition_v<KeyDefinition>>> {
    using interface_type = Interface;
    using normalized_interface = normalized_type_t<Interface>;
    using key_domain = KeyDefinition;
    using one_lookup_entry =
        static_key_lookup_entry_t<normalized_interface, key_domain,
                                  ::dingo::one>;
    using many_lookup_entry =
        static_key_lookup_entry_t<normalized_interface, key_domain,
                                  ::dingo::many>;
    using selected_cardinality =
        static_key_lookup_cardinality_t<normalized_interface, key_domain>;
    using lookup_entry = std::conditional_t<
        std::is_same_v<selected_cardinality, ::dingo::one> &&
            !std::is_void_v<one_lookup_entry>,
        one_lookup_entry,
        std::conditional_t<
            std::is_same_v<selected_cardinality, ::dingo::many> &&
                !std::is_void_v<many_lookup_entry>,
            many_lookup_entry, base_lookup_entry>>;
    static constexpr bool has_explicit_lookup = false;
    static constexpr bool has_explicit_collection_lookup =
        std::is_same_v<selected_cardinality, ::dingo::many> &&
        !std::is_void_v<many_lookup_entry>;
  };

  template <typename Interface, typename KeyDefinition>
  struct lookup_index_route_impl<
      Interface, KeyDefinition,
      std::enable_if_t<
          !detail::is_static_lookup_key_definition_v<KeyDefinition>>> {
    using normalized_interface = normalized_type_t<Interface>;
    using key_domain = KeyDefinition;
    using lookup_entry =
        detail::selected_lookup_entry_t<normalized_interface, key_domain,
                                        normalized_lookup_entries>;
    using selected_cardinality =
        typename lookup_entry_cardinality<lookup_entry>::type;
    static constexpr bool has_explicit_lookup = !std::is_void_v<lookup_entry>;
    static constexpr bool has_explicit_collection_lookup =
        has_explicit_lookup &&
        std::is_same_v<selected_cardinality, ::dingo::many>;
  };

  template <typename Interface, typename Key>
  struct lookup_index_route
      : lookup_index_route_impl<Interface,
                                typename std::decay_t<Key>::definition_type> {
    using key_definition = typename std::decay_t<Key>::definition_type;
    using base = lookup_index_route_impl<Interface, key_definition>;
    using entry = typename base::lookup_entry;

    static decltype(auto) key(const Key &key) {
      if constexpr (std::is_same_v<entry, base_lookup_entry>) {
        return base_typed_key_lookup_key<typename base::interface_type, Key>();
      } else {
        return detail::lookup_backend_key(key);
      }
    }
  };

  template <typename T, typename LookupKey>
  static constexpr bool has_explicit_collection_lookup(const LookupKey &) {
    static_assert(detail::is_lookup_key_v<std::decay_t<LookupKey>>);
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using lookup_key_type = std::decay_t<LookupKey>;
    using route = lookup_index_route<resolve_type, lookup_key_type>;
    return route::has_explicit_collection_lookup;
  }

  template <typename Request, typename LookupKey>
  struct selected_runtime_binding {
    registry_type &registry;
    LookupKey &key;
    construction_scope scope;

    decltype(auto) select() {
      return registry.template source_select<typename Request::lookup_type>(
          key);
    }

    template <typename ResolveRequest, typename Selection>
    decltype(auto) resolve(runtime_context_type &context, Selection selection) {
      return registry.template source_resolve<Request>(scope, selection,
                                                       context, key);
    }
  };

  struct cache_update {
    detail::cache::entry *entry;
    runtime_context_type *context;
    const void *key;

    detail::cache::sink sink() noexcept { return {this, &publish}; }

    static void publish(void *state, void *address) {
      auto &update = *reinterpret_cast<cache_update *>(state);
      assert(update.entry != nullptr);
      update.context->on_rollback(
          [entry = update.entry]() noexcept { *entry = {}; });
      *update.entry = {update.key, address};
    }
  };

  template <typename Request, bool MayAutoConstruct, typename LookupKey>
  struct missing_runtime_binding {
    registry_type &registry;
    LookupKey &key;
    construction_scope scope;

    template <typename ResolveRequest>
    typename request_type<ResolveRequest>::interface_type
    resolve(runtime_context_type &context) {
      using result_type = typename request_type<ResolveRequest>::interface_type;
      return registry.template source_missing<Request, MayAutoConstruct,
                                              LookupKey, result_type>(
          scope, context, key);
    }
  };

  template <typename Request, typename LookupKey>
  runtime_selection source_select(LookupKey key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    auto *state = runtime_bindings();
    return state ? select_binding<Request>(*state, key)
                 : runtime_selection::miss();
  }

  template <typename Request, typename LookupKey>
  auto source_resolve(construction_scope scope, runtime_selection selection,
                      runtime_context_type &context, const LookupKey &) ->
      typename Request::interface_type {
    return resolve_binding<typename Request::lookup_type,
                           typename Request::interface_type>(selection, scope,
                                                             context);
  }

  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R source_missing(construction_scope scope, runtime_context_type &context,
                   const LookupKey &key) {
    using Type = typename Request::value_type;
    using user_type = typename Request::user_type;
    using lookup_key_type = std::decay_t<LookupKey>;

    if constexpr (MayAutoConstruct && collection_traits<R>::is_collection) {
      if (count_collection<R>(key) != 0) {
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

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t for_each_collection_entry(runtime_bindings_state &state,
                                        LookupKey key, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using lookup_type = normalized_type_t<resolve_type>;
    using lookup_key_type = std::decay_t<LookupKey>;
    using route = lookup_index_route<lookup_type, lookup_key_type>;

    if constexpr (route::has_explicit_collection_lookup ||
                  std::is_same_v<typename route::entry, base_lookup_entry>) {
      return for_each_lookup<typename route::entry>(state, route::key(key),
                                                    std::forward<Fn>(fn));
    } else {
      (void)fn;
      return 0;
    }
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_runtime_collection(T &results, construction_scope scope,
                                        runtime_context_type &context, Fn &&fn,
                                        LookupKey key) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    auto *state = runtime_bindings();
    if (!state) {
      return 0;
    }
    return for_each_collection_entry<T>(*state, key, [&](auto &entry) {
      fn(results, resolve_collection_type<resolve_type>(scope, entry.binding(),
                                                        context));
    });
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    std::size_t result = 0;
    if (auto *state = runtime_bindings()) {
      for_each_collection_entry<T>(*state, key, [&](auto &) { ++result; });
    }
    return result;
  }

private:
  template <typename Request, typename LookupKey>
  runtime_selection select_binding(runtime_bindings_state &state,
                                   LookupKey &key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    using request = request_type<Request>;
    using lookup_type = typename request::value_type;
    using exact_type = typename request::exact_type;
    if constexpr (!detail::is_static_lookup_key_definition_v<LookupKey>) {
      using lookup_route = lookup_index_route<lookup_type, LookupKey>;
      if constexpr (!lookup_route::has_explicit_lookup &&
                    !std::is_same_v<lookup_type, exact_type>) {
        using exact_route = lookup_index_route<exact_type, LookupKey>;
        if constexpr (exact_route::has_explicit_lookup) {
          return select_binding_at_interface<Request, LookupKey, exact_type>(
              state, key);
        }
      }
      return select_binding_at_interface<Request, LookupKey, lookup_type>(state,
                                                                          key);
    } else {
      auto selection =
          select_binding_at_interface<Request, LookupKey, exact_type>(state,
                                                                      key);
      if constexpr (!std::is_same_v<lookup_type, exact_type>) {
        if (!selection.found()) {
          selection =
              select_binding_at_interface<Request, LookupKey, lookup_type>(
                  state, key);
        }
      }
      return selection;
    }
  }

  template <typename Request, typename LookupKey, typename Interface>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  runtime_selection select_binding_at_interface(runtime_bindings_state &state,
                                                LookupKey &key) {
    using route = lookup_index_route<Interface, std::decay_t<LookupKey>>;
    using lookup_entry = typename route::entry;
    if constexpr (std::is_void_v<lookup_entry>) {
      if constexpr (!std::is_same_v<void, ParentRegistry> &&
                    !std::is_base_of_v<registry_type, resolve_root_type>) {
        return runtime_selection::miss();
      } else {
        static_assert(!std::is_void_v<lookup_entry>,
                      "keyed registration or request has no matching "
                      "dingo lookup definition for interface/key");
      }
    } else {
      return select_lookup<lookup_entry>(state, route::key(key));
    }
  }

  runtime_selection make_selection(runtime_lookup_value *selected,
                                   bool ambiguous) {
    if (ambiguous) {
      return runtime_selection::ambiguity();
    }
    return detail::make_runtime_selection<runtime_binding_interface_type>(
        selected ? &selected->binding() : nullptr);
  }

  template <typename LookupEntry, typename Key>
  runtime_selection select_lookup(runtime_bindings_state &state,
                                  const Key &key) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    auto match =
        detail::lookup_find_singular<LookupEntry, runtime_lookup_value>(index,
                                                                        key);
    return make_selection(match.value, match.ambiguous);
  }

  template <typename LookupEntry, typename Key, typename Fn>
  std::size_t for_each_lookup(runtime_bindings_state &state, const Key &key,
                              Fn &&fn) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    return detail::lookup_for_each<LookupEntry>(index, key,
                                                std::forward<Fn>(fn));
  }

public:
  template <typename TypeInterface, typename TypeStorage, typename BindingState,
            typename LookupKey>
  using runtime_registration_binding_t = detail::keyed_binding_identity<
      LookupKey, runtime_binding<container_type,
                                 typename annotated_traits<TypeInterface>::type,
                                 TypeStorage, BindingState>>;

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

  template <typename InterfaceList, typename TypeStorage, typename BindingState,
            typename LookupKey>
  using runtime_binding_value_owner_t =
      typename runtime_binding_value_owner<InterfaceList, TypeStorage,
                                           BindingState, LookupKey>::type;

private:
  template <typename... TypeArgs, typename Parent, typename Arg,
            typename... KeyValueArgs>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  auto prepare_binding(Parent *parent, Arg &&arg,
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
        registration_container_type<registration, bindings_type, Parent>;
    (void)arg;
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
          validate_supplied_runtime_registration_keys<interface_types,
                                                      KeyValueArgs...>();
        }
        using interface_type = type_list_head_t<interface_types>;

        using owner_type =
            runtime_binding_value_impl<runtime_registration_binding_t<
                interface_type, storage_type, runtime_binding_state_type,
                registration_key>>;
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
            validate_supplied_runtime_registration_keys<interface_types,
                                                        KeyValueArgs...>();
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
            commit_registration<interface_type, storage_type>(
                state, value_factory, transaction, registration_key{},
                key_values...);
          });
          transaction.commit();
          return container_proxy<owner_type>(binding_result);
        } else {
          return invalid_registration_return<instance_container_type>();
        }
      }
    } else {
      return invalid_registration_return<instance_container_type>();
    }
  }

  template <typename Entry, typename Tuple, std::size_t Index = 0>
  static decltype(auto) qualified_registration_key_value(Tuple &keys) {
    using tuple_type = std::remove_reference_t<Tuple>;
    if constexpr (Index >= std::tuple_size_v<tuple_type>) {
      return unqualified_registration_key_value<Entry>(keys);
    } else if constexpr (runtime_registration_arg_matches_qualified_entry<
                             Entry, std::decay_t<std::tuple_element_t<
                                        Index, tuple_type>>>::value) {
      return std::get<Index>(keys).value();
    } else {
      return qualified_registration_key_value<Entry, Tuple, Index + 1>(keys);
    }
  }

  template <typename Entry, typename Tuple, std::size_t Index = 0>
  static decltype(auto) unqualified_registration_key_value(Tuple &keys) {
    using tuple_type = std::remove_reference_t<Tuple>;
    using key_type = typename runtime_lookup_entry_traits<Entry>::key_type;
    if constexpr (Index >= std::tuple_size_v<tuple_type>) {
      static_assert(Index < std::tuple_size_v<tuple_type>,
                    "missing runtime key value for runtime-key lookup "
                    "registration");
    } else {
      using element_type =
          std::decay_t<std::tuple_element_t<Index, tuple_type>>;
      using element_traits = runtime_registration_arg_traits<element_type>;
      if constexpr (std::is_same_v<typename element_traits::key_type,
                                   key_type> &&
                    std::is_void_v<typename element_traits::interface_type>) {
        return std::get<Index>(keys).value();
      } else {
        return unqualified_registration_key_value<Entry, Tuple, Index + 1>(
            keys);
      }
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename LookupEntry, typename ValueFactory, typename KeyArg>
  auto insert_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     const LookupKey &lookup_key, const KeyArg &key_arg) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();

    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto [handle, inserted] = index.try_emplace(
          key_arg, value_factory.template make<TypeInterface>());
      if (!inserted) {
        throw detail::make_lookup_already_registered_exception<
            TypeInterface, typename TypeStorage::type>(lookup_key, key_arg);
      }
      return handle;
    } else {
      return index.emplace(key_arg,
                           value_factory.template make<TypeInterface>());
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename ValueFactory, typename KeyResolver>
  void commit_lookup(runtime_bindings_state &, ValueFactory &,
                     runtime_transaction_type &, KeyResolver &&,
                     const LookupKey &, type_list<>) {}

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename Head, typename... Tail, typename ValueFactory,
            typename KeyResolver>
  void commit_lookup(runtime_bindings_state &state, ValueFactory &value_factory,
                     runtime_transaction_type &transaction,
                     KeyResolver &&key_resolver, const LookupKey &lookup_key,
                     type_list<Head, Tail...>) {
    static_assert(ValueFactory::shared_owner || sizeof...(Tail) == 0,
                  "unique runtime binding owner can only be inserted into one "
                  "lookup");
    decltype(auto) key_arg = key_resolver(type_list_iterator<Head>{});
    auto handle = insert_lookup<TypeInterface, TypeStorage, LookupKey, Head>(
        state, value_factory, lookup_key, key_arg);
    record_lookup_rollback<Head>(transaction, state, handle, key_arg);
    commit_lookup<TypeInterface, TypeStorage, LookupKey>(
        state, value_factory, transaction,
        std::forward<KeyResolver>(key_resolver), lookup_key,
        type_list<Tail...>{});
  }

  template <typename LookupEntry, typename Key, typename Binding>
  static void erase_lookup_binding(runtime_bindings_state &state,
                                   const Key &key, Binding *binding) noexcept {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto it = index.find(key);
      if (it != index.end()) {
        auto &value = detail::lookup_iterator_value(it);
        if (std::addressof(value.binding()) == binding) {
          index.erase(it);
          return;
        }
      }
    } else {
      auto [first, last] = index.equal_range(key);
      for (auto it = first; it != last; ++it) {
        auto &value = detail::lookup_iterator_value(it);
        if (std::addressof(value.binding()) == binding) {
          index.erase(it);
          return;
        }
      }
    }
    assert(false && "runtime lookup binding was not found");
  }

  template <typename LookupEntry, typename Handle, typename Key>
  static void record_lookup_rollback(runtime_transaction_type &transaction,
                                     runtime_bindings_state &state,
                                     Handle handle, const Key &key) {
    auto &value = detail::lookup_iterator_value(handle);
    auto *binding = std::addressof(value.binding());
    try {
      transaction.on_rollback(
          [&state, key = std::decay_t<Key>(key), binding]() noexcept {
            erase_lookup_binding<LookupEntry>(state, key, binding);
          });
    } catch (...) {
      erase_lookup_binding<LookupEntry>(state, key, binding);
      throw;
    }
  }

  template <typename Owner> struct unique_lookup_value_factory {
    static constexpr bool shared_owner = false;
    Owner *owner;

    template <typename TypeInterface> runtime_lookup_value make() {
      auto view = owner->template view<TypeInterface>();
      return runtime_lookup_value(view);
    }
  };

  template <typename Owner> struct shared_lookup_value_factory {
    static constexpr bool shared_owner = true;
    Owner *owner;

    template <typename TypeInterface> runtime_lookup_value make() {
      return runtime_lookup_value(owner->template view<TypeInterface>());
    }
  };

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename Cardinality, typename ValueFactory>
  void commit_static_lookup(runtime_bindings_state &state,
                            ValueFactory &value_factory,
                            runtime_transaction_type &transaction,
                            LookupKey lookup_key) {
    using key_domain = typename LookupKey::definition_type;
    using explicit_lookup_entry =
        static_key_lookup_entry_t<TypeInterface, key_domain, Cardinality>;
    using route = lookup_index_route<TypeInterface, LookupKey>;
    using routed_lookup_entry = typename route::entry;
    using routed_cardinality =
        typename lookup_entry_cardinality<routed_lookup_entry>::type;
    if constexpr (!std::is_void_v<explicit_lookup_entry> ||
                  std::is_same_v<Cardinality, ::dingo::one> ||
                  std::is_same_v<routed_cardinality, Cardinality>) {
      static_assert(!std::is_void_v<routed_lookup_entry>);
      static_assert(lookup_entry_indexed_v<routed_lookup_entry>);
      auto key_resolver = [&](auto) { return route::key(lookup_key); };
      commit_lookup<TypeInterface, TypeStorage, LookupKey>(
          state, value_factory, transaction, key_resolver, lookup_key,
          type_list<routed_lookup_entry>{});
    } else {
      static_assert(std::is_same_v<Cardinality, ::dingo::one>,
                    "static-key many lookup registrations require an explicit "
                    "dingo lookup definition");
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupKey,
            typename ValueFactory, typename... KeyValueArgs>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  void commit_registration(runtime_bindings_state &state,
                           ValueFactory &value_factory,
                           runtime_transaction_type &transaction,
                           LookupKey lookup_key, KeyValueArgs &&...key_values) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();
    ((void)key_values, ...);
    using key_domain = typename LookupKey::definition_type;
    using lookup_cardinality_type =
        static_key_lookup_cardinality_t<TypeInterface, key_domain>;

    if constexpr (sizeof...(KeyValueArgs) > 0) {
      using lookup_entries =
          runtime_lookup_entries_for_interface_t<TypeInterface>;
      if constexpr (type_list_size_v<lookup_entries> > 0) {
        validate_required_runtime_registration_keys<lookup_entries,
                                                    KeyValueArgs...>();
        auto key_value_tuple = std::forward_as_tuple(key_values...);
        auto key_resolver = [&](auto element) -> decltype(auto) {
          using entry_type = typename decltype(element)::type;
          return qualified_registration_key_value<entry_type>(key_value_tuple);
        };
        commit_lookup<TypeInterface, TypeStorage, LookupKey>(
            state, value_factory, transaction, key_resolver, lookup_key,
            lookup_entries{});
      } else {
        commit_static_lookup<TypeInterface, TypeStorage, LookupKey,
                             lookup_cardinality_type>(state, value_factory,
                                                      transaction, lookup_key);
      }
    } else {
      commit_static_lookup<TypeInterface, TypeStorage, LookupKey,
                           lookup_cardinality_type>(state, value_factory,
                                                    transaction, lookup_key);
    }
  }

  template <bool SharedOwner, typename TypeInterface, typename TypeStorage,
            typename Owner, typename Parent, typename LookupKey,
            typename KeyValueTuple, typename... Args>
  container_proxy<Owner> commit_binding(Parent *parent, LookupKey lookup_key,
                                        KeyValueTuple &&key_values,
                                        Args &&...args) {
    inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
    runtime_transaction_type transaction(runtime(), scratch);
    auto &state = ensure_runtime_bindings(transaction, parent);
    auto &binding_owner = *transaction.template construct_persistent<Owner>(
        this, std::forward<Args>(args)...);
    auto *binding_result = std::addressof(binding_owner);
    if constexpr (SharedOwner) {
      shared_lookup_value_factory<Owner> value_factory{
          std::addressof(binding_owner)};
      std::apply(
          [&](auto &&...key_value_args) {
            commit_registration<TypeInterface, TypeStorage>(
                state, value_factory, transaction, lookup_key,
                std::forward<decltype(key_value_args)>(key_value_args)...);
          },
          std::forward<KeyValueTuple>(key_values));
    } else {
      unique_lookup_value_factory<Owner> value_factory{
          std::addressof(binding_owner)};
      std::apply(
          [&](auto &&...key_value_args) {
            commit_registration<TypeInterface, TypeStorage>(
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
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type>
  R resolve_impl(construction_scope scope, runtime_context_type &context,
                 LookupKey key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    using Type = typename Request::value_type;
    static_assert(!std::is_const_v<Type>);

    using lookup_key_type = LookupKey;
    selected_runtime_binding<Request, lookup_key_type> selected{*this, key,
                                                                scope};
    missing_runtime_binding<Request, MayAutoConstruct, lookup_key_type> missing{
        *this, key, scope};
    auto sources = detail::make_selected_binding_sources(selected, missing);
    return detail::resolve_from_binding_sources<typename Request::lookup_type,
                                                R>(context, sources);
  }

  template <typename Request>
  decltype(auto) auto_construct(construction_scope scope,
                                runtime_context_type &context) {
    using Type = typename Request::value_type;

    static_assert(is_complete<Type>::value,
                  "auto-construction requires a complete type");

    using type_detection = detail::automatic;
    if constexpr (dependency_observation_enabled) {
      auto perform = [&]() -> decltype(auto) {
        return context.template construct<typename Request::interface_type,
                                          type_detection>(scope,
                                                          *resolve_root());
      };
      return detail::observe_dependencies<Type>(context.dependency_observer(),
                                                {}, container_id(), perform);
    } else {
      return context
          .template construct<typename Request::interface_type, type_detection>(
              scope, *resolve_root());
    }
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

  template <typename Request, typename Result, typename Binding,
            typename Context>
  Result resolve(construction_scope scope, Binding &binding, Context &context,
                 detail::cache::sink cache = {}) {
    return ::dingo::resolve_binding_request<Request, rtti_type>(scope, binding,
                                                                context, cache);
  }

  template <typename T, typename Binding, typename Context>
  T resolve_collection_type(construction_scope scope, Binding &binding,
                            Context &context) {
    return ::dingo::resolve_binding_request<T, rtti_type>(scope, binding,
                                                          context);
  }

  template <class Storage, class TypeInterface, class Type>
  void check_interface_requirements() {
    detail::interface_registration_requirements<Storage, TypeInterface,
                                                Type>::assert_valid();
  }

  parent_container_type *parent_ = nullptr;

  detail::runtime_data_holder<runtime_data_type, OwnsRuntimeData> runtime_data_;
};

} // namespace dingo
