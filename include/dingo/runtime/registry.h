//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/none.h>
#include <dingo/factory/callable.h>
#include <dingo/lookup/lookup_key.h>
#include <dingo/registration/type_registration.h>
#include <dingo/request/collection.h>
#include <dingo/runtime/registry_store.h>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

template <typename ContainerTraits, typename Allocator, typename ResolveRoot>
class DINGO_EMPTY_BASES runtime_registry
    : public allocator_base<Allocator>,
      protected detail::registry_store<ContainerTraits, Allocator> {
  template <typename> friend class ::dingo::resolution_context;
  template <typename, typename, typename> friend class runtime_binding_state;
  template <typename, typename, typename>
  friend class detail::runtime_container_impl;
  template <typename, typename, typename> friend class runtime_registry;
  using store_type = detail::registry_store<ContainerTraits, Allocator>;
  using registry_type =
      runtime_registry<ContainerTraits, Allocator, ResolveRoot>;
  using resolve_root_type = ResolveRoot;
  using container_type = resolve_root_type;
  using runtime_type = runtime_memory<Allocator>;
  using typename store_type::runtime_binding_interface_type;
  using typename store_type::runtime_context_type;
  using typename store_type::runtime_selection;
  using typename store_type::runtime_transaction_type;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using rtti_type = typename ContainerTraits::rtti_type;
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;

protected:
  using typename store_type::runtime_bindings_state;
  using typename store_type::runtime_data_type;
  using typename store_type::runtime_scope_state;
  template <typename TypeInterface, typename Interfaces>
  using runtime_lookup_entries_for_interface_t =
      typename store_type::template runtime_lookup_entries_for_interface_t<
          TypeInterface, Interfaces>;
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
  // The registry of a container, which owns the runtime data.
  runtime_registry(resolve_root_type *root,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), root_(root),
        runtime_data_(get_allocator()) {}

  // The registry of a level of the local bindings of a registration made in
  // owner. It borrows the runtime data of the root of owner.
  runtime_registry(detail::local_level_t, resolve_root_type *owner,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc),
        root_(owner->binding_store().resolve_root()), runtime_data_(owner) {}

private:
  registry_type &binding_store() { return *this; }

  // A container is its own root; a level of local bindings is not.
  bool owns_runtime_data() const noexcept {
    return root_ == static_cast<const resolve_root_type *>(this);
  }

  // The container a registration is made in, which its factory resolves
  // through and constructs its collections with: a container, or a level of
  // local bindings, which is a container of the same type.
  resolve_root_type &registration_owner() {
    return static_cast<resolve_root_type &>(*this);
  }

public:
  runtime_registry(const runtime_registry &) = delete;
  runtime_registry &operator=(const runtime_registry &) = delete;

  ~runtime_registry() = default;

protected:
  // Destroys the runtime data the registry owns. Its container calls it as it
  // is destroyed, while it can still tell whether it is its own root.
  void destroy_runtime_data() noexcept {
    runtime_data_.destroy(owns_runtime_data());
  }

public:
  allocator_type &get_allocator() {
    return allocator_base<allocator_type>::get_allocator();
  }

  // Registers TypeArgs, built by an optional factory argument and found by
  // the runtime key values that follow it.
  template <typename... TypeArgs, typename... Args>
  auto register_type(Args &&...args) {
    if constexpr (detail::are_runtime_registration_key_args_v<Args...>) {
      return this->template prepare_binding<TypeArgs...>(
          none_t{}, std::forward<Args>(args)...);
    } else {
      return register_with_argument<TypeArgs...>(std::forward<Args>(args)...);
    }
  }

  template <typename... TypeArgs, typename Fn>
  auto register_type_collection(Fn &&fn) {
    using registration = type_registration<TypeArgs...>;
    return register_type<TypeArgs...>(
        callable([this, collection_fn = std::forward<Fn>(fn)]() mutable {
          return registration_owner()
              .template construct_collection<
                  typename registration::storage_type::type>(collection_fn);
        }));
  }

  template <typename... TypeArgs> auto register_type_collection() {
    return register_type_collection<TypeArgs...>(
        detail::binding_collection_append{});
  }

private:
  template <typename... TypeArgs, typename Arg, typename... KeyArgs>
  auto register_with_argument(Arg &&arg, KeyArgs &&...keys) {
    static_assert(detail::are_runtime_registration_key_args_v<KeyArgs...>,
                  "register_type takes a factory argument followed by "
                  "runtime key values");
    return this->template prepare_binding<TypeArgs...>(
        std::forward<Arg>(arg), std::forward<KeyArgs>(keys)...);
  }

public:
protected:
  runtime_memory<allocator_type> &runtime() {
    return shared_runtime_data().runtime;
  }

  runtime_data_type &shared_runtime_data() {
    if (owns_runtime_data()) {
      return runtime_data_.get();
    }
    return borrow_runtime_data(resolve_root());
  }

  runtime_bindings_state *runtime_bindings() {
    if (owns_runtime_data()) {
      return std::addressof(shared_runtime_data().bindings);
    }
    auto *scope = runtime_scope();
    return scope != nullptr ? std::addressof(scope->bindings) : nullptr;
  }

  runtime_scope_state *runtime_scope() {
    assert(!owns_runtime_data());
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

  runtime_bindings_state &
  ensure_runtime_bindings(runtime_transaction_type &transaction) {
    if (owns_runtime_data()) {
      return *runtime_bindings();
    }
    if (auto *scope = runtime_scope()) {
      return scope->bindings;
    }

    auto &data = shared_runtime_data();
    auto *state =
        transaction.template construct_persistent<runtime_scope_state>(
            get_allocator(), static_cast<void *>(&registration_owner()));
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

  // The container a level of local bindings falls back to: the container,
  // or the level, the registration it belongs to is made in. None for a
  // container.
  resolve_root_type *owner_level() const noexcept {
    return owns_runtime_data() ? nullptr : runtime_data_.owner();
  }

  // The container that owns the runtime data.
  resolve_root_type *resolve_root() { return root_; }

protected:
  // A lookup the traits do not declare fails as it compiles, in a level of
  // local bindings as in its container: they have the same traits.
  template <typename Value, typename Exact, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  runtime_selection select_binding(LookupKey key) {
    return store_type::template select_binding<Value, Exact, false>(
        runtime_bindings(), std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    using collection_type = collection_traits<T>;
    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");
    const std::size_t count = count_collection<T>(key);
    // A declared collection lookup with no rows is a valid empty collection.
    if (count == 0 &&
        !lookup_index_route<typename collection_type::resolve_type,
                            LookupKey>::has_explicit_collection_lookup) {
      throw detail::make_collection_type_not_found_exception<
          T, typename collection_type::resolve_type>();
    }
    T results;
    collection_type::reserve(results, count);
    append_collection(scope, results, context, std::forward<Fn>(fn),
                      std::move(key));
    return results;
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    return store_type::template count_collection<T>(runtime_bindings(),
                                                    std::move(key));
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

public:
  template <typename TypeInterface, typename TypeStorage, typename BindingState>
  using runtime_registration_binding_t =
      runtime_binding<container_type, unannotated_t<TypeInterface>, TypeStorage,
                      BindingState>;

  template <typename InterfaceList, typename TypeStorage, typename BindingState>
  struct runtime_binding_value_owner;

  template <typename... TypeInterfaces, typename TypeStorage,
            typename BindingState>
  struct runtime_binding_value_owner<type_list<TypeInterfaces...>, TypeStorage,
                                     BindingState> {
    using type = runtime_binding_value_impl<runtime_registration_binding_t<
        TypeInterfaces, TypeStorage, BindingState>...>;
  };

  template <typename TypeInterface, typename TypeStorage, typename BindingState>
  struct runtime_binding_value_owner<type_list<TypeInterface>, TypeStorage,
                                     BindingState> {
    using type = runtime_registration_binding_t<TypeInterface, TypeStorage,
                                                BindingState>;
  };

  template <typename InterfaceList, typename TypeStorage, typename BindingState>
  using runtime_binding_value_owner_t =
      typename runtime_binding_value_owner<InterfaceList, TypeStorage,
                                           BindingState>::type;

private:
  template <typename... TypeArgs, typename Arg, typename... KeyValueArgs>
  // Registration is cold; keep its type-specific setup out of callers.
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_NOINLINE auto prepare_binding(Arg &&arg, KeyValueArgs &&...key_values) {
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
    static_assert(std::is_void_v<bindings_type>,
                  "runtime registrations cannot declare compile-time "
                  "bindings<...>; use static_container");
    using instance_container_type = resolve_root_type;
    (void)std::addressof(arg);
    using interface_types = typename binding_model::interface_types;
    static_assert(binding_model::storage_tag_is_complete,
                  "registered storage tag must be complete and specialize "
                  "dingo::scope_traits; include the corresponding "
                  "dingo/storage header");
    static_assert(!binding_model::storage_tag_is_complete ||
                      binding_model::stored,
                  "the scope does not store this type");
    if constexpr (!binding_model::stored) {
      return store_type::template invalid_registration_return<
          instance_container_type>();
    } else {
      using storage_type = typename binding_model::storage_type;
      using registration_requirements = typename binding_model::requirements;
      using registration_key = typename binding_model::key_type;

      registration_requirements::assert_valid();

      using runtime_binding_state_type =
          runtime_binding_state<registry_type, instance_container_type,
                                detail::binding_cell_t<binding_model>>;

      if constexpr (!binding_model::valid) {
        return store_type::template invalid_registration_return<
            instance_container_type>();
      } else {
        if constexpr (sizeof...(KeyValueArgs) > 0) {
          store_type::template validate_supplied_runtime_registration_keys<
              interface_types, KeyValueArgs...>();
        }
        // One interface is bound by the binding itself; several share the
        // state of the registration through one binding per interface.
        constexpr bool single = type_list_size_v<interface_types> == 1;
        using owner_type = runtime_binding_value_owner_t<
            interface_types, storage_type,
            std::conditional_t<single, runtime_binding_state_type,
                               runtime_binding_state_type *>>;
        static constexpr bool shared_owner =
            !single ||
            (sizeof...(KeyValueArgs) > 0 &&
             type_list_size_v<runtime_lookup_entries_for_interface_t<
                     type_list_head_t<interface_types>, interface_types>> > 1);
        return commit_binding<shared_owner, interface_types, storage_type,
                              owner_type, runtime_binding_state_type>(
            registration_key{}, std::forward<Arg>(arg),
            std::forward<KeyValueArgs>(key_values)...);
      }
    }
  }

  // Constructs a binding in the transaction, from the registered factory
  // argument when there is one.
  template <typename T, typename Arg>
  T *construct_binding(runtime_transaction_type &transaction, Arg &&arg) {
    if constexpr (is_none_v<std::decay_t<Arg>>) {
      (void)arg;
      return transaction.template construct_persistent<T>(this);
    } else {
      return transaction.template construct_persistent<T>(
          this, std::forward<Arg>(arg));
    }
  }

  // Constructs the owner of a registration and commits it to the lookups of
  // its interfaces in one transaction.
  template <bool SharedOwner, typename Interfaces, typename TypeStorage,
            typename Owner, typename State, typename LookupKey, typename Arg,
            typename... KeyValueArgs>
  DINGO_NOINLINE container_proxy<Owner>
  commit_binding(LookupKey lookup_key, Arg &&arg,
                 KeyValueArgs &&...key_values) {
    inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch;
    runtime_transaction_type transaction(runtime(), scratch);
    auto &state = ensure_runtime_bindings(transaction);
    auto commit = [&](auto &value_factory) {
      for_each(Interfaces{}, [&](auto element) {
        using interface_type = typename decltype(element)::type;
        this->template commit_registration<interface_type, TypeStorage,
                                           Interfaces>(
            state, value_factory, transaction, lookup_key, key_values...);
      });
    };
    Owner *owner = nullptr;
    if constexpr (type_list_size_v<Interfaces> == 1) {
      owner = construct_binding<Owner>(transaction, std::forward<Arg>(arg));
      direct_lookup_value_factory<SharedOwner, Owner> value_factory{owner};
      commit(value_factory);
    } else {
      auto *data =
          construct_binding<State>(transaction, std::forward<Arg>(arg));
      owner = transaction.template construct_persistent<Owner>(data);
      shared_lookup_value_factory<Owner> value_factory{owner};
      commit(value_factory);
    }
    transaction.commit();
    return container_proxy<Owner>(owner);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  friend struct detail::request_resolution;

  template <typename Request, typename LookupKey>
  runtime_selection select_request(const LookupKey &key) {
    static_assert(!std::is_const_v<typename Request::value_type>);
    return select_binding<typename Request::value_type,
                          typename Request::exact_type>(key);
  }

  template <typename Request, typename R>
  R resolve_selected(runtime_selection selection, construction_scope scope,
                     runtime_context_type &context) {
    return store_type::template resolve_binding<
        typename Request::interface_type, R>(selection, scope, context);
  }

  template <typename Request>
  [[noreturn]] void ambiguous(runtime_context_type &context) {
    detail::throw_type_ambiguous(
        detail::described_type_v<typename Request::lookup_type>, context);
  }

  template <typename R, typename LookupKey>
  R construct_request_collection(construction_scope scope,
                                 runtime_context_type &context, LookupKey key) {
    return construct_collection<R>(
        scope, context, detail::binding_collection_append{}, std::move(key));
  }

#ifdef _MSC_VER
#pragma warning(pop)
#endif

  resolve_root_type *root_ = nullptr;

  detail::runtime_data_holder<runtime_data_type, resolve_root_type>
      runtime_data_;
};

} // namespace dingo
