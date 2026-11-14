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

  // The level of the local bindings of a registration, made as it is first
  // asked for.
  class container_proxy {
  public:
    using container_type = resolve_root_type;

    container_proxy(runtime_binding_header *header, runtime_registry *registry)
        : header_(header), registry_(registry) {}

    container_type &get() const {
      if (header_->level == nullptr) {
        header_->level = std::addressof(
            registry_->runtime().template construct<container_type>(
                detail::local_level,
                static_cast<container_type *>(header_->container),
                registry_->get_allocator()));
      }
      return *static_cast<container_type *>(header_->level);
    }
    container_type *operator->() const { return std::addressof(get()); }
    operator container_type &() const { return get(); }

  private:
    runtime_binding_header *header_;
    runtime_registry *registry_;
  };

  // The edge of the binding of an interface of a registration.
  template <typename TypeInterface, typename TypeStorage>
  using runtime_registration_edge_t =
      runtime_binding_edge<container_type, unannotated_t<TypeInterface>,
                           TypeStorage>;

private:
  template <typename... TypeArgs, typename Arg, typename... KeyValueArgs>
  // Registration is cold; keep its type-specific setup out of callers.
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_NOINLINE auto prepare_binding(Arg &&arg, KeyValueArgs &&...key_values) {
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
    (void)std::addressof(arg);
    using interface_types = typename binding_model::interface_types;
    constexpr bool valid = detail::registration_checked_v<registration>;
    if constexpr (!binding_model::stored) {
      return store_type::template invalid_registration_return<
          container_proxy>();
    } else {
      using storage_type = typename binding_model::storage_type;
      using registration_key = typename binding_model::key_type;

      if constexpr (!valid) {
        return store_type::template invalid_registration_return<
            container_proxy>();
      } else {
        if constexpr (sizeof...(KeyValueArgs) > 0) {
          store_type::template validate_supplied_runtime_registration_keys<
              interface_types, KeyValueArgs...>();
        }
        return commit_registration<storage_type>(
            registration_key{}, std::forward<Arg>(arg), interface_types{},
            key_values...);
      }
    }
  }

  // The transaction of a registration, with its scratch memory, made and
  // destroyed out of line, once for a container, not for every registration.
  class registration_frame {
  public:
    DINGO_NOINLINE explicit registration_frame(runtime_type &runtime)
        : transaction_(runtime, scratch_) {}
    DINGO_NOINLINE ~registration_frame() = default;
    registration_frame(const registration_frame &) = delete;
    registration_frame &operator=(const registration_frame &) = delete;

    runtime_transaction_type &transaction() noexcept { return transaction_; }

  private:
    inline_arena<DINGO_CONTEXT_ARENA_BUFFER_SIZE> scratch_;
    runtime_transaction_type transaction_;
  };

  using runtime_binding_node_type =
      typename runtime_binding_interface_type::node_type;

  // A registration as constant data: the layout of its record, which
  // destroys its cell, and the node and the descriptor of the binding of each
  // of its interfaces.
  struct registration_node {
    std::size_t size;
    std::size_t alignment;
    std::size_t cell_offset;
    std::size_t bindings_offset;
    void (*destroy)(void *) noexcept;
    const runtime_binding_node_type *const *nodes;
    const type_descriptor *interfaces;
    std::size_t count;
    type_descriptor storage;
  };

  // Constructs the cell of a registration in its record, from the registered
  // factory argument when there is one.
  template <typename Cell, typename Arg>
  static void construct_cell(void *cell, void *arg) {
    if constexpr (is_none_v<std::decay_t<Arg>>) {
      (void)arg;
      ::new (cell) Cell();
    } else {
      ::new (cell) Cell(
          std::forward<Arg>(*static_cast<std::remove_reference_t<Arg> *>(arg)));
    }
  }

  template <typename TypeStorage, typename Record, typename... TypeInterfaces>
  struct registration_node_of {
    static constexpr const runtime_binding_node_type *nodes[] = {
        &runtime_registration_edge_t<TypeInterfaces, TypeStorage>::node...};
    static constexpr type_descriptor interfaces[] = {
        detail::described_type_v<TypeInterfaces>...};
    static constexpr registration_node value{
        Record::size,
        Record::alignment,
        detail::runtime_cell_offset_v<detail::binding_cell<TypeStorage>>,
        Record::bindings_offset,
        &Record::destroy,
        nodes,
        interfaces,
        sizeof...(TypeInterfaces),
        detail::described_type_v<typename TypeStorage::type>};
  };

  using lookup_operations = typename store_type::lookup_operations;

  // A lookup a registration commits the binding of one of its interfaces to:
  // the index of the binding of the interface, the lookup, the key, and the
  // descriptor of the type of the key, which names the registration if the
  // lookup rejects it.
  struct lookup_insert {
    std::size_t binding;
    const lookup_operations *lookup;
    const void *key;
    type_descriptor key_type;
  };

  // The lookups of an interface of a registration: of the static key of the
  // registration, as the container routes it, or of each key value it is
  // registered with, to each lookup of the interface that is keyed by it.
  template <std::size_t Index, typename TypeInterface, typename Interfaces,
            typename LookupKey, typename... KeyValueArgs>
  static void lookup_inserts_of(lookup_insert *&out,
                                const LookupKey &lookup_key,
                                const void *static_key,
                                std::tuple<KeyValueArgs &...> &key_values) {
    using key_type = std::decay_t<LookupKey>;
    using entries =
        typename store_type::template runtime_lookup_entries_for_interface_t<
            TypeInterface, Interfaces>;
    if constexpr (sizeof...(KeyValueArgs) > 0 &&
                  store_type::template runtime_lookup_entries_for_interface<
                      TypeInterface, Interfaces,
                      typename store_type::normalized_lookup_entries>::
                      defers_to_normalized) {
      // A keyed request of an interface that defers to its normalized
      // interface selects the lookup of that one, which commits it.
      (void)out, (void)lookup_key, (void)static_key, (void)key_values;
    } else if constexpr (sizeof...(KeyValueArgs) == 0 ||
                         type_list_size_v<entries> == 0) {
      // The lookup of the static key of the registration, as the container
      // routes it, also of an interface that no key value keys.
      (void)key_values;
      using route =
          typename store_type::template lookup_index_route<TypeInterface,
                                                           key_type>;
      using route_key = std::decay_t<decltype(route::key(lookup_key))>;
      *out++ = {
          Index,
          &store_type::template route_operations<TypeInterface, key_type>(),
          static_key, detail::registration_key_type<route_key>(lookup_key)};
    } else {
      (void)static_key;
      store_type::template validate_required_runtime_registration_keys<
          entries, std::decay_t<KeyValueArgs>...>();
      for_each(entries{}, [&](auto element) {
        using entry = typename decltype(element)::type;
        const auto &key =
            store_type::template registration_key_value<entry>(key_values);
        using key_arg = std::decay_t<decltype(key)>;
        *out++ = {
            Index,
            &store_type::template lookup_operations_of<entry, key_arg>::value,
            std::addressof(key),
            detail::registration_key_type<key_arg>(lookup_key)};
      });
    }
  }

  template <typename TypeInterface, typename Interfaces, typename... Args>
  static constexpr std::size_t lookup_insert_count() {
    using entries =
        typename store_type::template runtime_lookup_entries_for_interface_t<
            TypeInterface, Interfaces>;
    if constexpr (sizeof...(Args) == 0 || type_list_size_v<entries> == 0) {
      return 1;
    } else {
      return type_list_size_v<entries>;
    }
  }

  // Registers a type: the typed part of a registration is its node, the
  // construction of its cell from the registered factory argument, and the
  // keys of the lookups of its interfaces; the commit is of the node, by one
  // function of the container. Arg is the type of the factory argument as it
  // was passed.
  template <typename TypeStorage, typename LookupKey, typename Arg,
            typename... TypeInterfaces, typename... KeyValueArgs>
  container_proxy commit_registration(LookupKey lookup_key, Arg &&arg,
                                      type_list<TypeInterfaces...>,
                                      KeyValueArgs &...key_values) {
    using interfaces = type_list<TypeInterfaces...>;
    using cell_type = detail::binding_cell<TypeStorage>;
    using record = detail::runtime_binding_record<
        cell_type, runtime_binding_interface_type, sizeof...(TypeInterfaces)>;
    (detail::interface_registration_requirements<
         TypeStorage, unannotated_t<TypeInterfaces>,
         typename TypeStorage::type>::assert_valid(),
     ...);
    // The keys of a static key, which a lookup is routed to by the type of
    // each interface: they live as long as the commit.
    const auto static_keys = std::make_tuple(
        store_type::template lookup_index_route<
            TypeInterfaces, std::decay_t<LookupKey>>::key(lookup_key)...);
    std::tuple<KeyValueArgs &...> keys(key_values...);
    constexpr std::size_t count =
        (lookup_insert_count<TypeInterfaces, interfaces, KeyValueArgs...>() +
         ... + 0);
    lookup_insert inserts[count + 1] = {};
    lookup_insert *out = inserts;
    insert_all<TypeInterfaces...>(out, lookup_key, static_keys, keys,
                                  std::index_sequence_for<TypeInterfaces...>{});
    return commit_record(
        registration_node_of<TypeStorage, record, TypeInterfaces...>::value,
        inserts, static_cast<std::size_t>(out - inserts),
        &construct_cell<cell_type, Arg>,
        const_cast<void *>(
            static_cast<const volatile void *>(std::addressof(arg))));
  }

  template <typename... TypeInterfaces, typename LookupKey, typename StaticKeys,
            typename KeyValues, std::size_t... Indexes>
  static void insert_all(lookup_insert *&out, const LookupKey &lookup_key,
                         const StaticKeys &static_keys, KeyValues &key_values,
                         std::index_sequence<Indexes...>) {
    (lookup_inserts_of<Indexes, TypeInterfaces, type_list<TypeInterfaces...>>(
         out, lookup_key, std::addressof(std::get<Indexes>(static_keys)),
         key_values),
     ...);
  }

  // Commits a registration of its node, in one transaction: constructs its
  // record, its header, its cell, and the binding of each of its interfaces,
  // and inserts each binding into the lookups of its interface. It is of no
  // registration type.
  DINGO_NOINLINE container_proxy commit_record(
      const registration_node &node, const lookup_insert *inserts,
      std::size_t count, void (*construct)(void *, void *), void *arg) {
    registration_frame frame(runtime());
    auto &transaction = frame.transaction();
    auto &state = ensure_runtime_bindings(transaction);
    auto *header = static_cast<runtime_binding_header *>(
        transaction.construct_persistent_erased(
            node.size, node.alignment, node.destroy, [&](void *place) {
              auto *made = ::new (place)
                  runtime_binding_header{{nullptr, nullptr},
                                         std::addressof(registration_owner()),
                                         nullptr};
              construct(static_cast<unsigned char *>(place) + node.cell_offset,
                        arg);
              for (std::size_t index = 0; index < node.count; ++index) {
                ::new (binding_of(node, *made, index))
                    runtime_binding_interface_type(node.nodes[index], made);
              }
            }));
    for (std::size_t index = 0; index < count; ++index) {
      const lookup_insert &insert = inserts[index];
      insert.lookup->insert(
          state, transaction,
          typename store_type::runtime_lookup_value(
              binding_of(node, *header, insert.binding)),
          insert.key,
          {node.interfaces[insert.binding], node.storage, insert.key_type});
    }
    transaction.commit();
    return container_proxy(header, this);
  }

  static runtime_binding_interface_type *
  binding_of(const registration_node &node, runtime_binding_header &header,
             std::size_t index) noexcept {
    return reinterpret_cast<runtime_binding_interface_type *>(
               reinterpret_cast<unsigned char *>(&header) +
               node.bindings_offset) +
           index;
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  friend struct detail::request_resolution;

  // A request as data: the lookup and the key of its exact interface and of
  // its normalized interface, whether it is selected by its exact interface
  // first, and whether it falls back to its normalized interface when its
  // exact interface binds nothing.
  struct lookup_request {
    const lookup_operations *exact;
    const void *exact_key;
    const lookup_operations *value;
    const void *value_key;
    bool exact_first;
    bool falls_back;
  };

  // Selects the binding of a request of its data. It is of no request type.
  runtime_selection select_record(const lookup_request &request) {
    auto *state = runtime_bindings();
    if (state == nullptr) {
      return runtime_selection::miss();
    }
    if (!request.exact_first) {
      return request.value->find(*state, request.value_key);
    }
    using lookup_at = std::pair<const lookup_operations *, const void *>;
    return detail::select_exact_or_normalized(
        [&](const lookup_at &at) { return at.first->find(*state, at.second); },
        lookup_at(request.exact, request.exact_key),
        lookup_at(request.value, request.value_key), request.falls_back);
  }

  // Passes fn the data of a request, while its keys exist: the lookups its
  // interfaces are routed to, as the container declares them. A static key
  // selects the exact interface first, and the normalized one when the exact
  // one binds nothing. A key value selects by the exact interface only when it
  // has a lookup of its own, falling back to the normalized one when that has
  // one too.
  template <typename Request, typename LookupKey, typename Fn>
  static decltype(auto) with_lookup_request(const LookupKey &key, Fn &&fn) {
    using key_type = std::decay_t<LookupKey>;
    using exact = typename Request::exact_type;
    using value = typename Request::value_type;
    using exact_route =
        typename store_type::template lookup_index_route<exact, key_type>;
    using value_route =
        typename store_type::template lookup_index_route<value, key_type>;
    if constexpr (detail::is_static_lookup_key_definition_v<key_type>) {
      decltype(auto) exact_key = exact_route::key(key);
      decltype(auto) value_key = value_route::key(key);
      return fn(lookup_request{
          &store_type::template route_operations<exact, key_type>(),
          std::addressof(exact_key),
          &store_type::template route_operations<value, key_type>(),
          std::addressof(value_key), true, !std::is_same_v<value, exact>});
    } else if constexpr (!std::is_same_v<value, exact> &&
                         exact_route::has_explicit_lookup &&
                         !std::is_same_v<typename exact_route::entry,
                                         typename value_route::entry>) {
      decltype(auto) exact_key = exact_route::key(key);
      if constexpr (value_route::has_explicit_lookup) {
        decltype(auto) value_key = value_route::key(key);
        return fn(lookup_request{
            &store_type::template route_operations<exact, key_type>(),
            std::addressof(exact_key),
            &store_type::template route_operations<value, key_type>(),
            std::addressof(value_key), true, true});
      } else {
        return fn(lookup_request{
            &store_type::template route_operations<exact, key_type>(),
            std::addressof(exact_key), nullptr, nullptr, true, false});
      }
    } else {
      decltype(auto) value_key = value_route::key(key);
      return fn(lookup_request{
          nullptr, nullptr,
          &store_type::template route_operations<value, key_type>(),
          std::addressof(value_key), false, false});
    }
  }

  template <typename Request, typename LookupKey>
  runtime_selection select_request(const LookupKey &key) {
    static_assert(!std::is_const_v<typename Request::value_type>);
    return with_lookup_request<Request>(key,
                                        [this](const lookup_request &request) {
                                          return select_record(request);
                                        });
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
