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
          container_proxy>();
    } else {
      using storage_type = typename binding_model::storage_type;
      using registration_requirements = typename binding_model::requirements;
      using registration_key = typename binding_model::key_type;

      registration_requirements::assert_valid();

      if constexpr (!binding_model::valid) {
        return store_type::template invalid_registration_return<
            container_proxy>();
      } else {
        if constexpr (sizeof...(KeyValueArgs) > 0) {
          store_type::template validate_supplied_runtime_registration_keys<
              interface_types, KeyValueArgs...>();
        }
        if constexpr (sizeof...(KeyValueArgs) == 0 &&
                      commits_by_base_lookup<interface_types,
                                             registration_key>()) {
          return commit_by_base_lookup<interface_types, storage_type>(
              registration_key{}, std::forward<Arg>(arg));
        } else {
          return commit_binding<interface_types, storage_type>(
              registration_key{}, std::forward<Arg>(arg),
              std::forward<KeyValueArgs>(key_values)...);
        }
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

  // The bindings of the interfaces of a registration, in its record, which
  // the lookups of the interfaces point to.
  template <typename Interfaces, typename Record> struct record_bindings {
    static constexpr bool shared_owner = true;
    runtime_binding_header *header;

    template <typename TypeInterface>
    static constexpr std::size_t index_of(type_list<>) {
      return 0;
    }

    template <typename TypeInterface, typename Head, typename... Tail>
    static constexpr std::size_t index_of(type_list<Head, Tail...>) {
      if constexpr (std::is_same_v<unannotated_t<Head>,
                                   unannotated_t<TypeInterface>>) {
        return 0;
      } else {
        return 1 + index_of<TypeInterface>(type_list<Tail...>{});
      }
    }

    template <typename TypeInterface>
    typename store_type::runtime_lookup_value make() {
      return typename store_type::runtime_lookup_value(
          Record::binding(*header, index_of<TypeInterface>(Interfaces{})));
    }
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

  // Whether every interface of a registration is committed to the base
  // lookup, by its type and the type of its static key.
  template <typename Interfaces, typename LookupKey>
  static constexpr bool commits_by_base_lookup() {
    return commits_by_base_lookup<LookupKey>(Interfaces{});
  }

  template <typename LookupKey, typename... TypeInterfaces>
  static constexpr bool commits_by_base_lookup(type_list<TypeInterfaces...>) {
    using base_entry = typename store_type::base_lookup_entry;
    return (std::is_same_v<typename store_type::template lookup_index_route<
                               TypeInterfaces, LookupKey>::entry,
                           base_entry> &&
            ...);
  }

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

  // Registers through the base lookup: the typed part of the registration is
  // its node, the construction of its cell and the keys of its interfaces,
  // and the commit is of the node.
  template <typename Interfaces, typename TypeStorage, typename LookupKey,
            typename Arg>
  container_proxy commit_by_base_lookup(LookupKey lookup_key, Arg &&arg) {
    return commit_interfaces_by_base_lookup<TypeStorage, LookupKey, Arg>(
        lookup_key, arg, Interfaces{});
  }

  // Arg is the type of the registered factory argument, as it was passed.
  template <typename TypeStorage, typename LookupKey, typename Arg,
            typename... TypeInterfaces>
  container_proxy
  commit_interfaces_by_base_lookup(LookupKey lookup_key,
                                   std::remove_reference_t<Arg> &arg,
                                   type_list<TypeInterfaces...>) {
    using cell_type = detail::binding_cell<TypeStorage>;
    using record = detail::runtime_binding_record<
        cell_type, runtime_binding_interface_type, sizeof...(TypeInterfaces)>;
    (detail::interface_registration_requirements<
         TypeStorage, unannotated_t<TypeInterfaces>,
         typename TypeStorage::type>::assert_valid(),
     ...);
    using base_key = detail::base_lookup_key<rtti_type>;
    const base_key keys[] = {
        store_type::template base_typed_key_lookup_key<TypeInterfaces,
                                                       LookupKey>()...};
    return commit_record(
        registration_node_of<TypeStorage, record, TypeInterfaces...>::value,
        keys, detail::registration_key_type<base_key>(lookup_key),
        &construct_cell<cell_type, Arg>,
        const_cast<void *>(
            static_cast<const volatile void *>(std::addressof(arg))));
  }

  // Commits a registration of its node, in one transaction: constructs its
  // record, its header, its cell, and the binding of each of its interfaces,
  // and inserts each binding into the base lookup under its key. It is of no
  // registration type.
  DINGO_NOINLINE container_proxy commit_record(
      const registration_node &node,
      const detail::base_lookup_key<rtti_type> *keys, type_descriptor key_type,
      void (*construct)(void *, void *), void *arg) {
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
    using base_entry = typename store_type::base_lookup_entry;
    for (std::size_t index = 0; index < node.count; ++index) {
      this->template insert_lookup<base_entry>(
          state, transaction,
          typename store_type::runtime_lookup_value(
              binding_of(node, *header, index)),
          keys[index], {node.interfaces[index], node.storage, key_type});
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

  // Constructs the record of a registration, its header, the cell of its
  // storage from the registered factory argument when there is one, and the
  // binding of each of its interfaces, and commits the bindings to the
  // lookups of their interfaces, in one transaction.
  template <typename Interfaces, typename TypeStorage, typename LookupKey,
            typename Arg, typename... KeyValueArgs>
  DINGO_NOINLINE container_proxy commit_binding(LookupKey lookup_key, Arg &&arg,
                                                KeyValueArgs &&...key_values) {
    using cell_type = detail::binding_cell<TypeStorage>;
    using record =
        detail::runtime_binding_record<cell_type,
                                       runtime_binding_interface_type,
                                       type_list_size_v<Interfaces>>;
    registration_frame frame(runtime());
    auto &transaction = frame.transaction();
    auto &state = ensure_runtime_bindings(transaction);
    auto *header = static_cast<runtime_binding_header *>(
        transaction.construct_persistent_erased(
            record::size, record::alignment, &record::destroy,
            [&](void *place) {
              auto *made = ::new (place)
                  runtime_binding_header{{nullptr, nullptr},
                                         std::addressof(registration_owner()),
                                         nullptr};
              void *cell = &detail::runtime_cell<cell_type>(*made);
              if constexpr (is_none_v<std::decay_t<Arg>>) {
                (void)arg;
                ::new (cell) cell_type();
              } else {
                ::new (cell) cell_type(std::forward<Arg>(arg));
              }
              std::size_t index = 0;
              for_each(Interfaces{}, [&](auto element) {
                using interface_type = typename decltype(element)::type;
                ::new (record::binding(*made, index++))
                    runtime_binding_interface_type(
                        &runtime_registration_edge_t<interface_type,
                                                     TypeStorage>::node,
                        made);
              });
            }));
    record_bindings<Interfaces, record> bindings{header};
    for_each(Interfaces{}, [&](auto element) {
      using interface_type = typename decltype(element)::type;
      this->template commit_registration<interface_type, TypeStorage,
                                         Interfaces>(
          state, bindings, transaction, lookup_key, key_values...);
    });
    transaction.commit();
    return container_proxy(header, this);
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  friend struct detail::request_resolution;

  using base_lookup_key_type = detail::base_lookup_key<rtti_type>;

  // A request a container selects through the base lookup: the key of its
  // interface, and of its normalized interface, which selects it when its
  // interface is not bound.
  struct base_request {
    base_lookup_key_type exact;
    base_lookup_key_type value;
    bool normalizes;
  };

  // Whether a request is selected through the base lookup: one of a type
  // with a static key and no lookup of its own, which is not a collection.
  template <typename Request, typename R, typename LookupKey>
  static constexpr bool selects_by_base_lookup() {
    using base_entry = typename store_type::base_lookup_entry;
    using key_type = std::decay_t<LookupKey>;
    if constexpr (collection_traits<R>::is_collection ||
                  !detail::is_static_lookup_key_definition_v<key_type>) {
      return false;
    } else {
      return std::is_same_v<typename store_type::template lookup_index_route<
                                typename Request::exact_type, key_type>::entry,
                            base_entry> &&
             std::is_same_v<typename store_type::template lookup_index_route<
                                typename Request::value_type, key_type>::entry,
                            base_entry>;
    }
  }

  template <typename Request, typename LookupKey>
  static base_request base_request_of() {
    using exact = typename Request::exact_type;
    using value = typename Request::value_type;
    return {store_type::template base_typed_key_lookup_key<exact, LookupKey>(),
            store_type::template base_typed_key_lookup_key<value, LookupKey>(),
            !std::is_same_v<value, exact>};
  }

  // Selects the binding of a request through the base lookup. It is of no
  // request type.
  runtime_selection select_base(const base_request &request) {
    auto *state = runtime_bindings();
    if (state == nullptr) {
      return runtime_selection::miss();
    }
    using base_entry = typename store_type::base_lookup_entry;
    return detail::select_exact_or_normalized(
        [&](const base_lookup_key_type &key) {
          return this->template select_lookup<base_entry>(*state, key);
        },
        request.exact, request.value, request.normalizes);
  }

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
