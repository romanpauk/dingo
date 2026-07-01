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
#include <dingo/core/keyed.h>
#include <dingo/core/none.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/memory/allocator.h>
#include <dingo/memory/static_allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/registration/requirements.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup_table.h>
#include <dingo/static/local_resolution.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/request_traits.h>
#include <dingo/type/type_list.h>
#include <dingo/view/view.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <new>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace dingo {
namespace detail {

template <typename StaticRegistry, typename ParentContainer>
class container_with_static_bindings;

template <typename Registry> struct runtime_lookup_identity_probe;

} // namespace detail

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
class runtime_registry : public allocator_base<Allocator> {
  friend class runtime_context;
  template <typename T, typename Context, typename Container>
  friend T detail::resolve_context_request(Context &, Container &);
  template <typename, typename> friend class detail::binding_resolution;
  template <typename, typename>
  friend class detail::container_with_static_bindings;
  template <typename, typename, typename> friend class runtime_container;
  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT>
  friend class runtime_registry;
  template <typename RegistryT>
  friend struct detail::runtime_lookup_identity_probe;

  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT>
  using rebind_t = runtime_registry<ContainerTraitsT, AllocatorT,
                                    ParentRegistryT, ResolveRootT>;
  using registry_type =
      runtime_registry<ContainerTraits, Allocator, ParentRegistry, ResolveRoot>;
  using resolve_root_type =
      std::conditional_t<std::is_same_v<void, ResolveRoot>, registry_type,
                         ResolveRoot>;
  using container_type = resolve_root_type;
  template <typename Registration>
  using registration_container_type =
      runtime_registry<typename ContainerTraits::template rebind_t<
                           type_list<typename ContainerTraits::tag_type,
                                     typename Registration::interface_type>>,
                       Allocator, resolve_root_type, resolve_root_type>;
  using parent_registry_type =
      std::conditional_t<std::is_same_v<void, ParentRegistry>, registry_type,
                         ParentRegistry>;

  static constexpr bool cache_enabled = ContainerTraits::cache_enabled;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using rtti_type = typename ContainerTraits::rtti_type;
  using view_definition_type = typename ContainerTraits::view_definition_type;
  runtime_registry() : allocator_base<allocator_type>(allocator_type()) {}

  runtime_registry(const allocator_type &alloc)
      : allocator_base<allocator_type>(alloc) {}

  runtime_registry(resolve_root_type *root,
                   const allocator_type &alloc = allocator_type())
      : allocator_base<allocator_type>(alloc), resolve_root_(root) {

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

  runtime_registry(const runtime_registry &) = delete;
  runtime_registry &operator=(const runtime_registry &) = delete;

  ~runtime_registry() { destroy_runtime_bindings(); }

  allocator_type &get_allocator() {
    return allocator_base<allocator_type>::get_allocator();
  }

protected:
  bool has_runtime_registrations() const { return runtime_bindings_present_; }
  struct runtime_bindings_state;

  runtime_bindings_state *runtime_bindings_if_present();

  const runtime_bindings_state *runtime_bindings_if_present() const;

  runtime_bindings_state &ensure_runtime_bindings();

  void destroy_runtime_bindings();

  resolve_root_type *resolve_root() {
    if constexpr (std::is_same_v<void, ResolveRoot> ||
                  std::is_same_v<resolve_root_type, registry_type>) {
      return this;
    } else if constexpr (std::is_base_of_v<registry_type, resolve_root_type>) {
      return static_cast<resolve_root_type *>(this);
    } else {
      return resolve_root_;
    }
  }

  const resolve_root_type *resolve_root() const {
    if constexpr (std::is_same_v<void, ResolveRoot> ||
                  std::is_same_v<resolve_root_type, registry_type>) {
      return this;
    } else if constexpr (std::is_base_of_v<registry_type, resolve_root_type>) {
      return static_cast<const resolve_root_type *>(this);
    } else {
      return resolve_root_;
    }
  }

public:
  template <typename... TypeArgs> auto &register_type() {
    return register_type_impl<TypeArgs...>(resolve_root(), none_t{}, none_t{});
  }

  template <typename... TypeArgs, typename Arg> auto &register_type(Arg &&arg) {
    return register_type_impl<TypeArgs...>(resolve_root(),
                                           std::forward<Arg>(arg), none_t{});
  }

  template <typename... TypeArgs, typename IdType>
  auto &register_indexed_type(IdType &&id) {
    return register_type_impl<TypeArgs...>(resolve_root(), none_t{},
                                           std::forward<IdType>(id));
  }

  template <typename... TypeArgs, typename Arg, typename IdType>
  auto &register_indexed_type(Arg &&arg, IdType &&id) {
    return register_type_impl<TypeArgs...>(
        resolve_root(), std::forward<Arg>(arg), std::forward<IdType>(id));
  }

  template <typename... TypeArgs, typename Fn>
  auto &register_type_collection(Fn &&fn) {
    using registration = type_registration<TypeArgs...>;
    return register_type<TypeArgs...>(
        callable([this, collection_fn = std::forward<Fn>(fn)]() mutable {
          return this->template construct_collection<
              typename registration::storage_type::type>(collection_fn);
        }));
  }

  template <typename... TypeArgs> auto &register_type_collection() {
    return register_type_collection<TypeArgs...>(
        detail::binding_collection_append{});
  }

  template <typename... TypeArgs, typename Parent, typename Arg,
            typename IdType>
  auto &emplace_type_binding(Parent &parent, Arg &&arg, IdType &&id) {
    return register_type_impl<TypeArgs...>(&parent, std::forward<Arg>(arg),
                                           std::forward<IdType>(id));
  }

protected:
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T>>
  R resolve(IdType &&id = IdType()) {
    return resolve_runtime_request<T>(std::forward<IdType>(id));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T>>
  R construct(Factory factory = Factory()) {
    return construct_runtime_request<T>(std::move(factory));
  }

  template <typename T> T construct_collection() {
    return construct_collection_runtime_request<T>();
  }

  template <typename T, typename Fn> T construct_collection(Fn &&fn) {
    return construct_collection_runtime_request<T>(std::forward<Fn>(fn));
  }

  template <typename T, typename Key> T construct_collection(key<Key>) {
    return construct_collection_runtime_request<T>(key<Key>{});
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key<Key>) {
    return construct_collection_runtime_request<T>(std::forward<Fn>(fn),
                                                   key<Key>{});
  }

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    return invoke_runtime_request<Signature>(std::forward<Callable>(callable));
  }

  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T>>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  R resolve_runtime_request(IdType &&id = IdType()) {
    if constexpr (detail::is_typed_key_v<IdType> &&
                  collection_traits<R>::is_collection) {
      return construct_collection_runtime_request<R>(std::decay_t<IdType>{});
    } else if constexpr (!is_none_v<std::decay_t<IdType>> &&
                         collection_traits<R>::is_collection) {
      return construct_collection_runtime_request<R>(
          detail::binding_collection_append{}, std::forward<IdType>(id));
    } else if constexpr (collection_traits<R>::is_collection) {
      return construct_collection_runtime_request<R>(
          detail::binding_collection_append{}, none_t{});
    } else {
      if constexpr (cache_enabled) {
        if constexpr (is_none_v<std::decay_t<IdType>>) {
          if (auto *state = runtime_bindings_if_present()) {
            void *cache = state->type_cache.template get<T>();
            if (cache) {
              return detail::convert_resolved_binding<request_interface_t<T>>(
                  cache);
            }
          }
        } else {
          if (auto *state = runtime_bindings_if_present()) {
            auto *data = runtime_source_bindings<T, IdType>(state);
            if (data) {
              auto selection = select_runtime_binding<T>(data, id);
              if (selection.found() && selection.state->cache) {
                return detail::convert_resolved_binding<request_interface_t<T>>(
                    selection.state->cache);
              }
            }
          }
        }
      }

      runtime_context context;
      return resolve_impl<T, true, false, false>(context,
                                                 std::forward<IdType>(id));
    }
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T>>
  R construct_runtime_request(Factory factory = Factory()) {
    runtime_context context;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      if (binding_status<T>() != detail::binding_selection_status::not_found) {
        if constexpr (!construct_normalized_request_v<T> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          return resolve<T, false>(context, none_t{});
        } else {
          return ::dingo::construct_request_or_wrap_normalized<T>(
              [&]() { return resolve<T, false>(context, none_t{}); },
              [&]() {
                return resolve<normalized_type_t<T>, false>(context, none_t{});
              });
        }
      } else if (binding_status<normalized_type_t<T>>() !=
                 detail::binding_selection_status::not_found) {
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          ::dingo::throw_missing_rvalue_conversion<T>(true, context);
        } else if constexpr (construct_normalized_request_v<T>) {
          return type_traits<std::decay_t<T>>::make(
              resolve<normalized_type_t<T>, false>(context, none_t{}));
        } else {
          return resolve<T, false>(context, none_t{});
        }
      }
    }

    if constexpr (construct_factory_request_v<T>) {
      return factory.template construct<R>(context, *resolve_root());
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             T>) {
      ::dingo::throw_missing_rvalue_conversion<T>(false, context);
    } else {
      return resolve<T, false>(context, none_t{});
    }
  }

  template <typename T> T construct_collection_runtime_request() {
    return construct_collection_runtime_request<T>(
        detail::binding_collection_append{});
  }

  template <typename T, typename Fn>
  T construct_collection_runtime_request(Fn &&fn) {
    return construct_collection_runtime_request<T>(std::forward<Fn>(fn),
                                                   none_t{});
  }

  template <typename T, typename Key>
  T construct_collection_runtime_request(key<Key>) {
    return construct_collection_runtime_request<T>(
        detail::binding_collection_append{}, key<Key>{});
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection_runtime_request(Fn &&fn, key<Key>) {
    return construct_collection_runtime_request_impl<T, Key>(
        std::forward<Fn>(fn));
  }

  template <typename T, typename Fn>
  T construct_collection_runtime_request(Fn &&fn, none_t) {
    return construct_collection_runtime_request_impl<T, void>(
        std::forward<Fn>(fn));
  }

  template <typename T, typename Fn, typename IdType,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  T construct_collection_runtime_request(Fn &&fn, IdType &&id) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    T results;
    runtime_context context;
    const std::size_t count = count_runtime_collection<T>(id);
    if (count == 0 &&
        !has_explicit_collection_lookup<T>(std::forward<IdType>(id))) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    collection_type::reserve(results, count);
    append_runtime_collection(results, context, std::forward<Fn>(fn),
                              std::forward<IdType>(id));
    return results;
  }

  template <typename T, typename Key, typename Fn>
  T construct_collection_runtime_request_impl(Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    T results;
    runtime_context context;
    const std::size_t count =
        count_runtime_collection<T>(collection_key<Key>());
    if (count == 0 &&
        !has_explicit_collection_lookup<T>(collection_key<Key>())) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    collection_type::reserve(results, count);
    append_runtime_collection(results, context, std::forward<Fn>(fn),
                              collection_key<Key>());
    return results;
  }

  template <typename Signature = void, typename Callable>
  auto invoke_runtime_request(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        detail::callable_dispatch_signature_t<Signature, callable_type>;

    runtime_context context;
    auto type_guard = context.template track_type<callable_type>();
    return detail::callable_invoke<dispatch_signature>::construct(
        std::forward<Callable>(callable), context, *resolve_root());
  }

  template <typename Request, typename Key = void>
  detail::binding_selection_status binding_status() {
    return binding_status_for_id<Request>(collection_key<Key>());
  }

  template <typename Request, typename IdType>
  detail::binding_selection_status binding_status_for_id(IdType &&id) {
    auto *state = runtime_bindings_if_present();
    auto *data = runtime_source_bindings<Request, IdType>(state);
    auto selection =
        select_runtime_binding<Request>(data, std::forward<IdType>(id));
    return selection.status;
  }

  template <typename T, typename Key = void, typename Fn>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn) {
    return append_runtime_collection(results, context, std::forward<Fn>(fn),
                                     collection_key<Key>());
  }

  template <typename T, typename Key = void> std::size_t count_collection() {
    return count_runtime_collection<T>(collection_key<Key>());
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve_request(runtime_context &context) {
    return resolve<T, RemoveRvalueReferences>(context, collection_key<Key>());
  }

  template <typename CachedT>
  static void store_type_cache(void *context, void *ptr) {
    static_cast<registry_type *>(context)
        ->ensure_runtime_bindings()
        .type_cache.template insert<CachedT>(ptr);
  }

  static void store_index_cache(void *context, void *ptr) {
    static_cast<binding_cache_state *>(context)->cache = ptr;
  }

  template <typename T> static T &invalid_registration_return();

  struct binding_cache_state {
    void *cache = nullptr;
  };

  struct registered_binding_entry;
  using normalized_view_entries =
      detail::normalize_lookup_definitions_t<view_definition_type>;
  using lookup_table_type =
      detail::runtime_lookup_table<rtti_type, allocator_type,
                                   registered_binding_entry,
                                   normalized_view_entries>;
  using lookup_identity = typename lookup_table_type::lookup_identity;
  using lookup_identity_key_domain = typename lookup_table_type::key_domain;
  using lookup_identity_cardinality = typename lookup_table_type::cardinality;

  template <typename Cardinality>
  static constexpr lookup_identity_cardinality lookup_cardinality() {
    return detail::runtime_lookup_cardinality_value<Cardinality>();
  }

  template <typename LookupEntry>
  using lookup_entry_cardinality =
      detail::runtime_lookup_entry_cardinality<LookupEntry>;

  struct registered_binding_entry : binding_cache_state {
    runtime_binding_ptr<runtime_binding_interface<container_type>> binding;
    typename rtti_type::type_index storage_type;
    std::optional<typename rtti_type::type_index> key_type;
    lookup_identity identity;

    registered_binding_entry(
        runtime_binding_ptr<runtime_binding_interface<container_type>>
            &&binding_ptr,
        typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        lookup_identity &&resolved_lookup_identity)
        : binding(std::move(binding_ptr)),
          storage_type(std::move(resolved_storage_type)),
          key_type(std::move(resolved_key_type)),
          identity(std::move(resolved_lookup_identity)) {}

    registered_binding_entry(const registered_binding_entry &) = delete;
    registered_binding_entry &
    operator=(const registered_binding_entry &) = delete;

    registered_binding_entry(registered_binding_entry &&other) noexcept
        : binding_cache_state{other.cache}, binding(std::move(other.binding)),
          storage_type(std::move(other.storage_type)),
          key_type(std::move(other.key_type)),
          identity(std::move(other.identity)) {
      other.cache = nullptr;
    }

    registered_binding_entry &
    operator=(registered_binding_entry &&other) noexcept {
      if (this != &other) {
        this->cache = other.cache;
        other.cache = nullptr;
        binding = std::move(other.binding);
        storage_type = std::move(other.storage_type);
        key_type = std::move(other.key_type);
        identity = std::move(other.identity);
      }
      return *this;
    }
  };

  template <typename Interface, typename KeyDomain, typename Cardinality>
  struct runtime_binding_slot {};

  template <typename Interface, typename Cardinality>
  using no_key_runtime_binding_slot =
      runtime_binding_slot<Interface, ::dingo::no_key, Cardinality>;

  template <typename Interface, typename Key, typename Cardinality>
  using typed_key_runtime_binding_slot = runtime_binding_slot<
      Interface, ::dingo::typed_key<detail::normalized_lookup_key_t<Key>>,
      Cardinality>;

  template <typename Interface, typename Key, typename Cardinality>
  using runtime_key_runtime_binding_slot = runtime_binding_slot<
      Interface, ::dingo::runtime_key<detail::normalized_lookup_key_t<Key>>,
      Cardinality>;

  struct runtime_type_bindings {
    static constexpr std::size_t inline_binding_storage_size = 512;
    static constexpr std::size_t inline_binding_storage_alignment =
        alignof(std::max_align_t);

    using entry_allocator = typename std::conditional_t<
        ::dingo::is_static_allocator_v<allocator_type>,
        std::allocator<registered_binding_entry>,
        typename std::allocator_traits<allocator_type>::template rebind_alloc<
            registered_binding_entry>>;
    using entry_list = std::list<registered_binding_entry, entry_allocator>;

    enum class slot_storage_kind {
      no_key,
      ordinary_one,
      ordinary_many,
      runtime_key
    };

    struct entry_handle {
      registered_binding_entry *ptr = nullptr;
      typename entry_list::iterator list_iterator{};
      bool inline_entry = false;
      slot_storage_kind storage_kind = slot_storage_kind::no_key;

      registered_binding_entry &operator*() const { return *ptr; }
    };

  private:
    struct no_key_slot_storage;
    template <detail::runtime_lookup_cardinality Cardinality>
    struct ordinary_selector_storage;
    template <detail::runtime_lookup_cardinality Cardinality,
              slot_storage_kind Kind>
    struct ordinary_slot_storage;
    using ordinary_one_slot_storage =
        ordinary_slot_storage<detail::runtime_lookup_cardinality::one,
                              slot_storage_kind::ordinary_one>;
    using ordinary_many_slot_storage =
        ordinary_slot_storage<detail::runtime_lookup_cardinality::many,
                              slot_storage_kind::ordinary_many>;
    struct runtime_key_slot_storage;

  public:
    struct no_key_slot_tag {};
    template <typename LookupEntry> struct ordinary_one_slot_tag {};
    template <typename LookupEntry> struct ordinary_many_slot_tag {};
    struct runtime_key_slot_tag {};

    template <typename Cardinality, typename LookupEntry>
    using ordinary_slot_tag_t =
        std::conditional_t<std::is_same_v<Cardinality, ::dingo::one>,
                           ordinary_one_slot_tag<LookupEntry>,
                           ordinary_many_slot_tag<LookupEntry>>;

    runtime_type_bindings(allocator_type &allocator, no_key_slot_tag)
        : storage_(std::in_place_type<no_key_slot_storage>, allocator) {
      validate_lookup_definitions();
    }

    template <typename LookupEntry>
    runtime_type_bindings(allocator_type &allocator,
                          ordinary_one_slot_tag<LookupEntry> tag)
        : storage_(std::in_place_type<ordinary_one_slot_storage>, allocator,
                   tag) {
      validate_lookup_definitions();
    }

    template <typename LookupEntry>
    runtime_type_bindings(allocator_type &allocator,
                          ordinary_many_slot_tag<LookupEntry> tag)
        : storage_(std::in_place_type<ordinary_many_slot_storage>, allocator,
                   tag) {
      validate_lookup_definitions();
    }

    runtime_type_bindings(allocator_type &allocator, runtime_key_slot_tag)
        : storage_(std::in_place_type<runtime_key_slot_storage>, allocator) {
      validate_lookup_definitions();
    }

    runtime_type_bindings(const runtime_type_bindings &) = delete;
    runtime_type_bindings &operator=(const runtime_type_bindings &) = delete;

    ~runtime_type_bindings() = default;

    bool has_no_key_singular_entry() const {
      if (auto *storage = std::get_if<no_key_slot_storage>(&storage_)) {
        return storage->has_no_key_singular_entry();
      }
      return false;
    }

    registered_binding_entry *no_key_singular_entry() {
      if (auto *storage = std::get_if<no_key_slot_storage>(&storage_)) {
        return storage->no_key_singular_entry();
      }
      return nullptr;
    }

    const registered_binding_entry *no_key_singular_entry() const {
      if (auto *storage = std::get_if<no_key_slot_storage>(&storage_)) {
        return storage->no_key_singular_entry();
      }
      return nullptr;
    }

    template <typename Binding, typename... Args>
    typename Binding::container_type &emplace_no_key_singular_entry(
        typename rtti_type::type_index resolved_storage_type,
        lookup_identity &&resolved_lookup_identity, Args &&...args) {
      return no_key_storage().template emplace_no_key_singular_entry<Binding>(
          std::move(resolved_storage_type), std::move(resolved_lookup_identity),
          std::forward<Args>(args)...);
    }

    template <typename Binding>
    void emplace_no_key_singular_entry(
        runtime_binding_ptr<runtime_binding_interface<container_type>>
            &&binding,
        typename rtti_type::type_index resolved_storage_type,
        lookup_identity &&resolved_lookup_identity) {
      no_key_storage().template emplace_no_key_singular_entry<Binding>(
          std::move(binding), std::move(resolved_storage_type),
          std::move(resolved_lookup_identity));
    }

    void erase_no_key_singular_entry() noexcept {
      if (auto *storage = std::get_if<no_key_slot_storage>(&storage_)) {
        storage->erase_no_key_singular_entry();
      }
    }

    template <typename Binding>
    entry_handle emplace_entry(
        Binding &&binding, typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        lookup_identity &&resolved_lookup_identity) {
      if (resolved_lookup_identity.domain ==
          lookup_table_type::key_domain::runtime_key) {
        return runtime_key_storage().emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::move(resolved_key_type), std::move(resolved_lookup_identity));
      }
      if (resolved_lookup_identity.lookup_cardinality ==
          lookup_table_type::cardinality::one) {
        return ordinary_one_storage().emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::move(resolved_key_type), std::move(resolved_lookup_identity));
      }
      return ordinary_many_storage().emplace_entry(
          std::forward<Binding>(binding), std::move(resolved_storage_type),
          std::move(resolved_key_type), std::move(resolved_lookup_identity));
    }

    template <typename Binding, typename... Args>
    std::pair<entry_handle, typename Binding::container_type *>
    emplace_inline_binding_entry(
        typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        lookup_identity &&resolved_lookup_identity, Args &&...args) {
      if (resolved_lookup_identity.domain ==
          lookup_table_type::key_domain::runtime_key) {
        return runtime_key_storage()
            .template emplace_inline_binding_entry<Binding>(
                std::move(resolved_storage_type), std::move(resolved_key_type),
                std::move(resolved_lookup_identity),
                std::forward<Args>(args)...);
      }
      if (resolved_lookup_identity.lookup_cardinality ==
          lookup_table_type::cardinality::one) {
        return ordinary_one_storage()
            .template emplace_inline_binding_entry<Binding>(
                std::move(resolved_storage_type), std::move(resolved_key_type),
                std::move(resolved_lookup_identity),
                std::forward<Args>(args)...);
      }
      return ordinary_many_storage()
          .template emplace_inline_binding_entry<Binding>(
              std::move(resolved_storage_type), std::move(resolved_key_type),
              std::move(resolved_lookup_identity), std::forward<Args>(args)...);
    }

    void erase_entry(entry_handle entry) {
      switch (entry.storage_kind) {
      case slot_storage_kind::ordinary_one:
        ordinary_one_storage().erase_entry(entry);
        return;
      case slot_storage_kind::ordinary_many:
        ordinary_many_storage().erase_entry(entry);
        return;
      case slot_storage_kind::runtime_key:
        runtime_key_storage().erase_entry(entry);
        return;
      case slot_storage_kind::no_key:
        break;
      }
    }

    bool insert_lookup_rows(entry_handle entry) {
      switch (entry.storage_kind) {
      case slot_storage_kind::ordinary_one:
        return ordinary_one_storage().insert_lookup_rows(entry);
      case slot_storage_kind::ordinary_many:
        return ordinary_many_storage().insert_lookup_rows(entry);
      case slot_storage_kind::runtime_key:
        return runtime_key_storage().insert_lookup_rows(entry);
      case slot_storage_kind::no_key:
        return false;
      }
      return false;
    }

    void erase_lookup_rows(registered_binding_entry *entry) {
      if (entry->identity.domain ==
          lookup_table_type::key_domain::runtime_key) {
        runtime_key_storage().erase_lookup_rows(entry);
      } else if (entry->identity.lookup_cardinality ==
                 lookup_table_type::cardinality::one) {
        ordinary_one_storage().erase_lookup_rows(entry);
      } else {
        ordinary_many_storage().erase_lookup_rows(entry);
      }
    }

    template <typename Interface, typename Cardinality>
    registered_binding_entry *find_singular_no_key(bool &ambiguous) {
      if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
        if (auto *storage = std::get_if<ordinary_one_slot_storage>(&storage_)) {
          return storage->find_singular(ambiguous);
        }
      } else {
        if (auto *storage =
                std::get_if<ordinary_many_slot_storage>(&storage_)) {
          return storage->find_singular(ambiguous);
        }
      }
      ambiguous = false;
      return nullptr;
    }

    template <typename Interface, typename Key, typename Cardinality>
    registered_binding_entry *find_singular_typed_key(bool &ambiguous) {
      if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
        if (auto *storage = std::get_if<ordinary_one_slot_storage>(&storage_)) {
          return storage->find_singular(ambiguous);
        }
      } else {
        if (auto *storage =
                std::get_if<ordinary_many_slot_storage>(&storage_)) {
          return storage->find_singular(ambiguous);
        }
      }
      ambiguous = false;
      return nullptr;
    }

    template <typename Interface, typename Key, typename Cardinality,
              typename LookupEntry, typename KeyValue>
    registered_binding_entry *find_singular_runtime_key(KeyValue &&key_value,
                                                        bool &ambiguous) {
      if (auto *storage = std::get_if<runtime_key_slot_storage>(&storage_)) {
        return storage->template find_singular<LookupEntry>(
            std::forward<KeyValue>(key_value), ambiguous);
      }
      ambiguous = false;
      return nullptr;
    }

    template <typename Interface, typename Cardinality, typename Fn>
    std::size_t for_each_no_key(Fn &&fn) {
      if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
        if (auto *storage = std::get_if<ordinary_one_slot_storage>(&storage_)) {
          return storage->for_each(std::forward<Fn>(fn));
        }
      } else {
        if (auto *storage =
                std::get_if<ordinary_many_slot_storage>(&storage_)) {
          return storage->for_each(std::forward<Fn>(fn));
        }
      }
      return 0;
    }

    template <typename Interface, typename Key, typename Cardinality,
              typename Fn>
    std::size_t for_each_typed_key(Fn &&fn) {
      if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
        if (auto *storage = std::get_if<ordinary_one_slot_storage>(&storage_)) {
          return storage->for_each(std::forward<Fn>(fn));
        }
      } else {
        if (auto *storage =
                std::get_if<ordinary_many_slot_storage>(&storage_)) {
          return storage->for_each(std::forward<Fn>(fn));
        }
      }
      return 0;
    }

    template <typename Interface, typename Key, typename Cardinality,
              typename LookupEntry, typename KeyValue, typename Fn>
    std::size_t for_each_runtime_key(KeyValue &&key_value, Fn &&fn) {
      if (auto *storage = std::get_if<runtime_key_slot_storage>(&storage_)) {
        return storage->template for_each<LookupEntry>(
            std::forward<KeyValue>(key_value), std::forward<Fn>(fn));
      }
      return 0;
    }

    template <typename Interface, typename Cardinality>
    std::size_t count_no_key() {
      if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
        if (auto *storage = std::get_if<ordinary_one_slot_storage>(&storage_)) {
          return storage->count();
        }
      } else {
        if (auto *storage =
                std::get_if<ordinary_many_slot_storage>(&storage_)) {
          return storage->count();
        }
      }
      return 0;
    }

    template <typename Interface, typename Key, typename Cardinality>
    std::size_t count_typed_key() {
      if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
        if (auto *storage = std::get_if<ordinary_one_slot_storage>(&storage_)) {
          return storage->count();
        }
      } else {
        if (auto *storage =
                std::get_if<ordinary_many_slot_storage>(&storage_)) {
          return storage->count();
        }
      }
      return 0;
    }

    template <typename Interface, typename Key, typename Cardinality,
              typename LookupEntry, typename KeyValue>
    std::size_t count_runtime_key(KeyValue &&key_value) {
      if (auto *storage = std::get_if<runtime_key_slot_storage>(&storage_)) {
        return storage->template count<LookupEntry>(
            std::forward<KeyValue>(key_value));
      }
      return 0;
    }

  private:
    static void validate_lookup_definitions() {
      using entries_type = normalized_view_entries;
      static_assert(!detail::has_duplicate_lookup_definition_v<entries_type>,
                    "duplicate dingo view definition for interface/key "
                    "domain/cardinality");
      static_assert(!detail::has_duplicate_lookup_slot_v<entries_type>,
                    "conflicting dingo view definitions for interface/key "
                    "domain");
    }

    template <typename Binding>
    static constexpr bool can_store_inline_binding =
        sizeof(Binding) <= inline_binding_storage_size &&
        alignof(Binding) <= inline_binding_storage_alignment;

    template <typename Binding, typename... Args>
    static std::pair<
        runtime_binding_ptr<runtime_binding_interface<container_type>>,
        typename Binding::container_type *>
    make_allocated_binding(allocator_type &allocator, Args &&...args) {
      auto alloc = allocator_traits::rebind<Binding>(allocator);
      auto *instance = allocator_traits::allocate(alloc, 1);
      try {
        allocator_traits::construct(alloc, instance,
                                    std::forward<Args>(args)...);
      } catch (...) {
        allocator_traits::deallocate(alloc, instance, 1);
        throw;
      }
      return {runtime_binding_ptr<runtime_binding_interface<container_type>>(
                  instance, &registry_type::template destroy_binding<Binding>),
              &instance->get_container()};
    }

    struct no_key_slot_storage {
      explicit no_key_slot_storage(allocator_type &allocator)
          : allocator_(std::addressof(allocator)) {}

      no_key_slot_storage(const no_key_slot_storage &) = delete;
      no_key_slot_storage &operator=(const no_key_slot_storage &) = delete;

      ~no_key_slot_storage() { erase_no_key_singular_entry(); }

      bool empty() const { return !no_key_singular_entry_constructed_; }

      allocator_type *allocator() const { return allocator_; }

      bool has_no_key_singular_entry() const {
        return no_key_singular_entry_constructed_;
      }

      registered_binding_entry *no_key_singular_entry() {
        return no_key_singular_entry_constructed_
                   ? std::launder(reinterpret_cast<registered_binding_entry *>(
                         no_key_singular_entry_storage_))
                   : nullptr;
      }

      const registered_binding_entry *no_key_singular_entry() const {
        return no_key_singular_entry_constructed_
                   ? std::launder(
                         reinterpret_cast<const registered_binding_entry *>(
                             no_key_singular_entry_storage_))
                   : nullptr;
      }

      template <typename Binding, typename... Args>
      typename Binding::container_type &emplace_no_key_singular_entry(
          typename rtti_type::type_index resolved_storage_type,
          lookup_identity &&resolved_lookup_identity, Args &&...args) {
        auto &&[binding, binding_container] =
            make_no_key_singular_binding<Binding>(std::forward<Args>(args)...);
        try {
          new (no_key_singular_entry_storage_) registered_binding_entry(
              std::move(binding), std::move(resolved_storage_type),
              std::nullopt, std::move(resolved_lookup_identity));
          no_key_singular_entry_constructed_ = true;
        } catch (...) {
          binding.reset();
          throw;
        }
        return *binding_container;
      }

      template <typename Binding>
      void emplace_no_key_singular_entry(
          runtime_binding_ptr<runtime_binding_interface<container_type>>
              &&binding,
          typename rtti_type::type_index resolved_storage_type,
          lookup_identity &&resolved_lookup_identity) {
        new (no_key_singular_entry_storage_) registered_binding_entry(
            std::move(binding), std::move(resolved_storage_type), std::nullopt,
            std::move(resolved_lookup_identity));
        no_key_singular_entry_constructed_ = true;
      }

      void erase_no_key_singular_entry() noexcept {
        if (auto *entry = no_key_singular_entry()) {
          entry->~registered_binding_entry();
          no_key_singular_entry_constructed_ = false;
        }
      }

      template <typename Binding, typename... Args>
      std::pair<runtime_binding_ptr<runtime_binding_interface<container_type>>,
                typename Binding::container_type *>
      make_no_key_singular_binding(Args &&...args) {
        if constexpr (can_store_inline_binding<Binding>) {
          auto *instance = reinterpret_cast<Binding *>(inline_binding_storage_);
          new (instance) Binding(std::forward<Args>(args)...);
          return {
              runtime_binding_ptr<runtime_binding_interface<container_type>>(
                  instance,
                  &registry_type::template destroy_inline_binding<Binding>),
              &instance->get_container()};
        }
        return runtime_type_bindings::make_allocated_binding<Binding>(
            *allocator_, std::forward<Args>(args)...);
      }

      allocator_type *allocator_;
      alignas(registered_binding_entry) std::byte
          no_key_singular_entry_storage_[sizeof(registered_binding_entry)];
      alignas(inline_binding_storage_alignment) std::byte
          inline_binding_storage_[inline_binding_storage_size];
      bool no_key_singular_entry_constructed_ = false;
    };

    struct lookup_entry_owner {
      explicit lookup_entry_owner(allocator_type &allocator)
          : allocator_(std::addressof(allocator)),
            entries(detail::make_lookup_storage_allocator<entry_allocator>(
                allocator)) {}

      lookup_entry_owner(const lookup_entry_owner &) = delete;
      lookup_entry_owner &operator=(const lookup_entry_owner &) = delete;

      ~lookup_entry_owner() { erase_inline_entry(); }

      bool empty() const {
        return !inline_entry_constructed_ && entries.empty();
      }

      allocator_type *allocator() const { return allocator_; }

      template <typename Binding>
      entry_handle emplace_entry(
          Binding &&binding,
          typename rtti_type::type_index resolved_storage_type,
          std::optional<typename rtti_type::type_index> resolved_key_type,
          lookup_identity &&resolved_lookup_identity) {
        if (!inline_entry_constructed_) {
          new (inline_entry_storage_) registered_binding_entry(
              std::forward<Binding>(binding), std::move(resolved_storage_type),
              std::move(resolved_key_type),
              std::move(resolved_lookup_identity));
          inline_entry_constructed_ = true;
          return {inline_entry(), {}, true};
        }

        entries.emplace_back(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::move(resolved_key_type), std::move(resolved_lookup_identity));
        auto entry = std::prev(entries.end());
        return {std::addressof(*entry), entry, false,
                slot_storage_kind::no_key};
      }

      template <typename Binding, typename... Args>
      std::pair<entry_handle, typename Binding::container_type *>
      emplace_inline_binding_entry(
          typename rtti_type::type_index resolved_storage_type,
          std::optional<typename rtti_type::type_index> resolved_key_type,
          lookup_identity &&resolved_lookup_identity, Args &&...args) {
        if (!inline_entry_constructed_ && can_store_inline_binding<Binding>) {
          auto *instance = reinterpret_cast<Binding *>(inline_binding_storage_);
          new (instance) Binding(std::forward<Args>(args)...);
          runtime_binding_ptr<runtime_binding_interface<container_type>>
              binding(instance,
                      &registry_type::template destroy_inline_binding<Binding>);
          auto *binding_container = std::addressof(instance->get_container());
          try {
            new (inline_entry_storage_) registered_binding_entry(
                std::move(binding), std::move(resolved_storage_type),
                std::move(resolved_key_type),
                std::move(resolved_lookup_identity));
            inline_entry_constructed_ = true;
          } catch (...) {
            binding.reset();
            throw;
          }
          return {{inline_entry(), {}, true, slot_storage_kind::no_key},
                  binding_container};
        }

        auto &&[binding, binding_container] =
            runtime_type_bindings::make_allocated_binding<Binding>(
                *allocator_, std::forward<Args>(args)...);
        try {
          return {emplace_entry(std::move(binding),
                                std::move(resolved_storage_type),
                                std::move(resolved_key_type),
                                std::move(resolved_lookup_identity)),
                  binding_container};
        } catch (...) {
          binding.reset();
          throw;
        }
      }

      void erase_entry(entry_handle entry) {
        if (entry.inline_entry) {
          erase_inline_entry();
          return;
        }
        entries.erase(entry.list_iterator);
      }

    private:
      allocator_type *allocator_;
      entry_list entries;

      registered_binding_entry *inline_entry() {
        return inline_entry_constructed_
                   ? std::launder(reinterpret_cast<registered_binding_entry *>(
                         inline_entry_storage_))
                   : nullptr;
      }

      void erase_inline_entry() noexcept {
        if (auto *entry = inline_entry()) {
          entry->~registered_binding_entry();
          inline_entry_constructed_ = false;
        }
      }

      alignas(inline_binding_storage_alignment) std::byte
          inline_binding_storage_[inline_binding_storage_size];
      alignas(registered_binding_entry) std::byte
          inline_entry_storage_[sizeof(registered_binding_entry)];
      bool inline_entry_constructed_ = false;
    };

    template <detail::runtime_lookup_cardinality Cardinality>
    using ordinary_selector_cardinality_t =
        std::conditional_t<Cardinality ==
                               detail::runtime_lookup_cardinality::one,
                           ::dingo::one, ::dingo::many>;

    template <detail::runtime_lookup_cardinality Cardinality>
    using ordinary_selector_lookup_entry =
        detail::lookup_entry<void, ::dingo::no_key,
                             ordinary_selector_cardinality_t<Cardinality>,
                             detail::no_lookup_backend>;

    template <detail::runtime_lookup_cardinality Cardinality>
    using ordinary_selector_storage_type =
        detail::selector_storage<ordinary_selector_lookup_entry<Cardinality>,
                                 registered_binding_entry, allocator_type>;

    template <detail::runtime_lookup_cardinality Cardinality>
    struct ordinary_selector_storage {
      using default_selector_storage_type =
          ordinary_selector_storage_type<Cardinality>;

      explicit ordinary_selector_storage(allocator_type &allocator)
          : ordinary_selector_storage(
                allocator, ordinary_selector_lookup_entry<Cardinality>{}) {}

      template <typename LookupEntry>
      ordinary_selector_storage(allocator_type &allocator, LookupEntry) {
        construct<LookupEntry>(allocator);
      }

      ordinary_selector_storage(const ordinary_selector_storage &) = delete;
      ordinary_selector_storage &
      operator=(const ordinary_selector_storage &) = delete;

      ~ordinary_selector_storage() { vtable_->destroy(storage_ptr()); }

      bool conflicts(const registered_binding_entry &entry) const {
        return vtable_->conflicts(storage_ptr(), entry);
      }

      void insert(registered_binding_entry &entry) {
        vtable_->insert(storage_ptr(), entry);
      }

      void erase(registered_binding_entry &entry) {
        vtable_->erase(storage_ptr(), entry);
      }

      registered_binding_entry *find_singular(bool &ambiguous) const {
        return vtable_->find_singular(storage_ptr(), ambiguous);
      }

      template <typename Fn> std::size_t for_each(Fn &&fn) const {
        using fn_type = std::remove_reference_t<Fn>;
        auto callback = [](void *context, registered_binding_entry &entry) {
          (*static_cast<fn_type *>(context))(entry);
        };
        return vtable_->for_each(
            storage_ptr(),
            const_cast<void *>(static_cast<const void *>(std::addressof(fn))),
            callback);
      }

      std::size_t count() const { return vtable_->count(storage_ptr()); }

    private:
      struct selector_vtable {
        void (*destroy)(void *) noexcept;
        bool (*conflicts)(const void *, const registered_binding_entry &);
        void (*insert)(void *, registered_binding_entry &);
        void (*erase)(void *, registered_binding_entry &);
        registered_binding_entry *(*find_singular)(const void *, bool &);
        std::size_t (*for_each)(const void *, void *,
                                void (*)(void *, registered_binding_entry &));
        std::size_t (*count)(const void *);
      };

      template <typename LookupEntry>
      using selector_storage_type =
          detail::selector_storage<LookupEntry, registered_binding_entry,
                                   allocator_type>;

      template <typename LookupEntry>
      void construct(allocator_type &allocator) {
        using storage_type = selector_storage_type<LookupEntry>;
        static_assert(sizeof(storage_type) <= selector_storage_size);
        static_assert(alignof(storage_type) <= selector_storage_alignment);
        new (storage_ptr()) storage_type(allocator);
        vtable_ = std::addressof(vtable<LookupEntry>());
      }

      void *storage_ptr() { return selector_storage_; }

      const void *storage_ptr() const { return selector_storage_; }

      template <typename LookupEntry> static const selector_vtable &vtable() {
        static const selector_vtable instance{
            [](void *storage) noexcept {
              auto *typed =
                  static_cast<selector_storage_type<LookupEntry> *>(storage);
              std::destroy_at(typed);
            },
            [](const void *storage, const registered_binding_entry &entry) {
              auto *typed =
                  static_cast<const selector_storage_type<LookupEntry> *>(
                      storage);
              return typed->conflicts(entry);
            },
            [](void *storage, registered_binding_entry &entry) {
              auto *typed =
                  static_cast<selector_storage_type<LookupEntry> *>(storage);
              typed->insert(entry);
            },
            [](void *storage, registered_binding_entry &entry) {
              auto *typed =
                  static_cast<selector_storage_type<LookupEntry> *>(storage);
              typed->erase(entry);
            },
            [](const void *storage, bool &ambiguous) {
              auto *typed =
                  static_cast<const selector_storage_type<LookupEntry> *>(
                      storage);
              return typed->find_singular(ambiguous);
            },
            [](const void *storage, void *context,
               void (*callback)(void *, registered_binding_entry &)) {
              auto *typed =
                  static_cast<const selector_storage_type<LookupEntry> *>(
                      storage);
              return typed->for_each(
                  [&](auto &entry) { callback(context, entry); });
            },
            [](const void *storage) {
              auto *typed =
                  static_cast<const selector_storage_type<LookupEntry> *>(
                      storage);
              return typed->count();
            }};
        return instance;
      }

      static constexpr std::size_t selector_storage_size =
          sizeof(default_selector_storage_type);
      static constexpr std::size_t selector_storage_alignment =
          alignof(default_selector_storage_type);

      alignas(selector_storage_alignment) std::byte
          selector_storage_[selector_storage_size];
      const selector_vtable *vtable_ = nullptr;
    };

    template <detail::runtime_lookup_cardinality Cardinality,
              slot_storage_kind Kind>
    struct ordinary_slot_storage {
      explicit ordinary_slot_storage(allocator_type &allocator)
          : owner(allocator), selector(allocator) {}

      template <typename LookupEntry>
      ordinary_slot_storage(allocator_type &allocator,
                            ordinary_one_slot_tag<LookupEntry>)
          : owner(allocator), selector(allocator, LookupEntry{}) {}

      template <typename LookupEntry>
      ordinary_slot_storage(allocator_type &allocator,
                            ordinary_many_slot_tag<LookupEntry>)
          : owner(allocator), selector(allocator, LookupEntry{}) {}

      ordinary_slot_storage(const ordinary_slot_storage &) = delete;
      ordinary_slot_storage &operator=(const ordinary_slot_storage &) = delete;

      bool empty() const { return owner.empty() && selector.count() == 0; }

      allocator_type *allocator() const { return owner.allocator(); }

      template <typename Binding>
      entry_handle emplace_entry(
          Binding &&binding,
          typename rtti_type::type_index resolved_storage_type,
          std::optional<typename rtti_type::type_index> resolved_key_type,
          lookup_identity &&resolved_lookup_identity) {
        auto handle = owner.emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::move(resolved_key_type), std::move(resolved_lookup_identity));
        handle.storage_kind = Kind;
        return handle;
      }

      template <typename Binding, typename... Args>
      std::pair<entry_handle, typename Binding::container_type *>
      emplace_inline_binding_entry(
          typename rtti_type::type_index resolved_storage_type,
          std::optional<typename rtti_type::type_index> resolved_key_type,
          lookup_identity &&resolved_lookup_identity, Args &&...args) {
        auto result = owner.template emplace_inline_binding_entry<Binding>(
            std::move(resolved_storage_type), std::move(resolved_key_type),
            std::move(resolved_lookup_identity), std::forward<Args>(args)...);
        result.first.storage_kind = Kind;
        return result;
      }

      void erase_entry(entry_handle entry) { owner.erase_entry(entry); }

      bool insert_lookup_rows(entry_handle entry) {
        if (selector.conflicts(*entry)) {
          return false;
        }
        try {
          selector.insert(*entry);
        } catch (...) {
          selector.erase(*entry);
          throw;
        }
        return true;
      }

      void erase_lookup_rows(registered_binding_entry *entry) {
        selector.erase(*entry);
      }

      registered_binding_entry *find_singular(bool &ambiguous) {
        return selector.find_singular(ambiguous);
      }

      template <typename Fn> std::size_t for_each(Fn &&fn) {
        return selector.for_each(std::forward<Fn>(fn));
      }

      std::size_t count() const { return selector.count(); }

      lookup_entry_owner owner;
      ordinary_selector_storage<Cardinality> selector;
    };

    struct runtime_key_slot_storage {
      using runtime_key_storages_type =
          typename lookup_table_type::template runtime_key_storages<
              typename detail::runtime_lookup_entries<
                  normalized_view_entries>::type>;

      explicit runtime_key_slot_storage(allocator_type &allocator)
          : owner(allocator), runtime_key_buckets(allocator) {}

      runtime_key_slot_storage(const runtime_key_slot_storage &) = delete;
      runtime_key_slot_storage &
      operator=(const runtime_key_slot_storage &) = delete;

      bool empty() const { return owner.empty(); }

      allocator_type *allocator() const { return owner.allocator(); }

      template <typename Binding>
      entry_handle emplace_entry(
          Binding &&binding,
          typename rtti_type::type_index resolved_storage_type,
          std::optional<typename rtti_type::type_index> resolved_key_type,
          lookup_identity &&resolved_lookup_identity) {
        auto handle = owner.emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::move(resolved_key_type), std::move(resolved_lookup_identity));
        handle.storage_kind = slot_storage_kind::runtime_key;
        return handle;
      }

      template <typename Binding, typename... Args>
      std::pair<entry_handle, typename Binding::container_type *>
      emplace_inline_binding_entry(
          typename rtti_type::type_index resolved_storage_type,
          std::optional<typename rtti_type::type_index> resolved_key_type,
          lookup_identity &&resolved_lookup_identity, Args &&...args) {
        auto result = owner.template emplace_inline_binding_entry<Binding>(
            std::move(resolved_storage_type), std::move(resolved_key_type),
            std::move(resolved_lookup_identity), std::forward<Args>(args)...);
        result.first.storage_kind = slot_storage_kind::runtime_key;
        return result;
      }

      void erase_entry(entry_handle entry) { owner.erase_entry(entry); }

      bool insert_lookup_rows(entry_handle entry) {
        auto &registered_entry = *entry;
        if (runtime_key_buckets.conflicts(registered_entry,
                                          registered_entry.identity)) {
          return false;
        }
        try {
          if (!runtime_key_buckets.insert(registered_entry,
                                          registered_entry.identity)) {
            return false;
          }
        } catch (...) {
          runtime_key_buckets.erase(registered_entry);
          throw;
        }
        return true;
      }

      void erase_lookup_rows(registered_binding_entry *entry) {
        runtime_key_buckets.erase(*entry);
      }

      template <typename LookupEntry, typename KeyValue>
      registered_binding_entry *find_singular(const KeyValue &key_value,
                                              bool &ambiguous) {
        return runtime_key_buckets.template find_singular<LookupEntry>(
            key_value, ambiguous);
      }

      template <typename LookupEntry, typename KeyValue, typename Fn>
      std::size_t for_each(const KeyValue &key_value, Fn &&fn) {
        return runtime_key_buckets.template for_each<LookupEntry>(
            key_value, std::forward<Fn>(fn));
      }

      template <typename LookupEntry, typename KeyValue>
      std::size_t count(const KeyValue &key_value) {
        return runtime_key_buckets.template count<LookupEntry>(key_value);
      }

      lookup_entry_owner owner;
      runtime_key_storages_type runtime_key_buckets;
    };

    no_key_slot_storage &no_key_storage() {
      if (auto *storage = std::get_if<no_key_slot_storage>(&storage_)) {
        return *storage;
      }
      if (auto *allocator = empty_storage_allocator()) {
        storage_.template emplace<no_key_slot_storage>(*allocator);
      }
      return std::get<no_key_slot_storage>(storage_);
    }

    ordinary_one_slot_storage &ordinary_one_storage() {
      if (auto *storage = std::get_if<ordinary_one_slot_storage>(&storage_)) {
        return *storage;
      }
      if (auto *allocator = empty_storage_allocator()) {
        storage_.template emplace<ordinary_one_slot_storage>(*allocator);
      }
      return std::get<ordinary_one_slot_storage>(storage_);
    }

    ordinary_many_slot_storage &ordinary_many_storage() {
      if (auto *storage = std::get_if<ordinary_many_slot_storage>(&storage_)) {
        return *storage;
      }
      if (auto *allocator = empty_storage_allocator()) {
        storage_.template emplace<ordinary_many_slot_storage>(*allocator);
      }
      return std::get<ordinary_many_slot_storage>(storage_);
    }

    runtime_key_slot_storage &runtime_key_storage() {
      if (auto *storage = std::get_if<runtime_key_slot_storage>(&storage_)) {
        return *storage;
      }
      if (auto *allocator = empty_storage_allocator()) {
        storage_.template emplace<runtime_key_slot_storage>(*allocator);
      }
      return std::get<runtime_key_slot_storage>(storage_);
    }

    allocator_type *empty_storage_allocator() {
      return std::visit(
          [](auto &storage) -> allocator_type * {
            return storage.empty() ? storage.allocator() : nullptr;
          },
          storage_);
    }

    std::variant<no_key_slot_storage, ordinary_one_slot_storage,
                 ordinary_many_slot_storage, runtime_key_slot_storage>
        storage_;
  };

  struct runtime_bindings_state {
    explicit runtime_bindings_state(allocator_type &allocator)
        : type_bindings(allocator), type_cache(allocator) {}

    typename ContainerTraits::template type_map_type<runtime_type_bindings,
                                                     allocator_type>
        type_bindings;
    typename ContainerTraits::template type_cache_type<void *, allocator_type>
        type_cache;
  };

  struct inline_runtime_bindings_state_storage {
    runtime_bindings_state *get() {
      return constructed_
                 ? std::launder(reinterpret_cast<runtime_bindings_state *>(
                       storage_))
                 : nullptr;
    }

    const runtime_bindings_state *get() const {
      return constructed_
                 ? std::launder(
                       reinterpret_cast<const runtime_bindings_state *>(
                           storage_))
                 : nullptr;
    }

    runtime_bindings_state &ensure(allocator_type &allocator) {
      if (!constructed_) {
        new (storage_) runtime_bindings_state(allocator);
        constructed_ = true;
      }
      return *std::launder(reinterpret_cast<runtime_bindings_state *>(storage_));
    }

    void destroy(allocator_type &) {
      if (auto *state = get()) {
        state->~runtime_bindings_state();
        constructed_ = false;
      }
    }

    alignas(runtime_bindings_state) std::byte
        storage_[sizeof(runtime_bindings_state)];
    bool constructed_ = false;
  };

  struct allocated_runtime_bindings_state_storage {
    runtime_bindings_state *get() { return state_; }
    const runtime_bindings_state *get() const { return state_; }

    runtime_bindings_state &ensure(allocator_type &allocator) {
      if (!state_) {
        auto alloc = allocator_traits::rebind<runtime_bindings_state>(allocator);
        auto *state = allocator_traits::allocate(alloc, 1);
        try {
          allocator_traits::construct(alloc, state, allocator);
        } catch (...) {
          allocator_traits::deallocate(alloc, state, 1);
          throw;
        }
        state_ = state;
      }
      return *state_;
    }

    void destroy(allocator_type &allocator) {
      if (state_) {
        auto alloc = allocator_traits::rebind<runtime_bindings_state>(allocator);
        allocator_traits::destroy(alloc, state_);
        allocator_traits::deallocate(alloc, state_, 1);
        state_ = nullptr;
      }
    }

    runtime_bindings_state *state_ = nullptr;
  };

  static constexpr bool use_inline_runtime_bindings_state =
      std::is_same_v<void, ParentRegistry> ||
      is_static_allocator_v<allocator_type>;
  using runtime_bindings_state_storage = std::conditional_t<
      use_inline_runtime_bindings_state, inline_runtime_bindings_state_storage,
      allocated_runtime_bindings_state_storage>;

  using runtime_binding_interface_type =
      runtime_binding_interface<container_type>;
  using runtime_selection =
      detail::runtime_binding_selection<runtime_binding_interface_type,
                                        binding_cache_state *>;

  static std::size_t
  lookup_identity_count(const registered_binding_entry &entry) {
    (void)entry;
    return 1;
  }

  static const lookup_identity &
  lookup_identity_at(const registered_binding_entry &entry, std::size_t index) {
    (void)index;
    return entry.identity;
  }

  static const typename rtti_type::type_index &
  lookup_identity_interface_type(const lookup_identity &identity) {
    return identity.interface_type;
  }

  static lookup_identity_key_domain
  lookup_identity_domain(const lookup_identity &identity) {
    return identity.domain;
  }

  static lookup_identity_cardinality
  lookup_identity_cardinality_value(const lookup_identity &identity) {
    return identity.lookup_cardinality;
  }

  static const std::optional<typename rtti_type::type_index> &
  lookup_identity_key_type(const lookup_identity &identity) {
    return identity.key_type;
  }

  static bool
  lookup_identity_has_runtime_key_storage(const lookup_identity &identity) {
    return static_cast<bool>(identity.runtime_key);
  }

  template <typename Key>
  using collection_key_t =
      std::conditional_t<std::is_void_v<Key>, none_t, key<Key>>;

protected:
  template <typename Key> static collection_key_t<Key> collection_key() {
    return {};
  }

  template <typename Interface, typename Cardinality>
  using no_key_lookup_entry_t =
      detail::selected_no_key_lookup_entry_t<Interface, Cardinality,
                                             normalized_view_entries>;

  template <typename Interface>
  static constexpr bool has_explicit_no_key_one_lookup_v =
      !std::is_void_v<no_key_lookup_entry_t<Interface, ::dingo::one>>;

  template <typename Interface>
  static constexpr bool has_explicit_no_key_many_lookup_v =
      !std::is_void_v<no_key_lookup_entry_t<Interface, ::dingo::many>>;

  template <typename Interface>
  using no_key_lookup_cardinality_t =
      std::conditional_t<has_explicit_no_key_many_lookup_v<Interface>,
                         ::dingo::many, ::dingo::one>;

  template <typename Interface, typename Cardinality>
  using no_key_ordinary_lookup_entry_t = std::conditional_t<
      std::is_void_v<no_key_lookup_entry_t<Interface, Cardinality>>,
      detail::lookup_entry<normalized_type_t<Interface>, ::dingo::no_key,
                           Cardinality, detail::no_lookup_backend>,
      no_key_lookup_entry_t<Interface, Cardinality>>;

  template <typename Interface, typename Key, typename Cardinality>
  using typed_key_lookup_entry_t =
      detail::selected_typed_key_lookup_entry_t<Interface, Key, Cardinality,
                                                normalized_view_entries>;

  template <typename Interface, typename Key>
  static constexpr bool has_explicit_typed_key_one_lookup_v =
      !std::is_void_v<typed_key_lookup_entry_t<Interface, Key, ::dingo::one>>;

  template <typename Interface, typename Key>
  static constexpr bool has_explicit_typed_key_many_lookup_v =
      !std::is_void_v<typed_key_lookup_entry_t<Interface, Key, ::dingo::many>>;

  template <typename Interface, typename Key>
  using typed_key_lookup_cardinality_t = std::conditional_t<
      !has_explicit_typed_key_one_lookup_v<Interface, Key> &&
          has_explicit_typed_key_many_lookup_v<Interface, Key>,
      ::dingo::many, ::dingo::one>;

  template <typename Interface, typename Key, typename Cardinality>
  using typed_key_ordinary_lookup_entry_t = std::conditional_t<
      std::is_void_v<typed_key_lookup_entry_t<Interface, Key, Cardinality>>,
      detail::lookup_entry<
          normalized_type_t<Interface>,
          ::dingo::typed_key<detail::normalized_lookup_key_t<Key>>, Cardinality,
          detail::no_lookup_backend>,
      typed_key_lookup_entry_t<Interface, Key, Cardinality>>;

  template <typename Interface, typename KeyDomain, typename Cardinality>
  struct selected_ordinary_lookup_entry;

  template <typename Interface, typename Cardinality>
  struct selected_ordinary_lookup_entry<Interface, ::dingo::no_key,
                                        Cardinality> {
    using type = no_key_ordinary_lookup_entry_t<Interface, Cardinality>;
  };

  template <typename Interface, typename Key, typename Cardinality>
  struct selected_ordinary_lookup_entry<Interface, ::dingo::typed_key<Key>,
                                        Cardinality> {
    using type = typed_key_ordinary_lookup_entry_t<Interface, Key, Cardinality>;
  };

  template <typename Interface, typename KeyDomain, typename Cardinality>
  using selected_ordinary_lookup_entry_t =
      typename selected_ordinary_lookup_entry<Interface, KeyDomain,
                                              Cardinality>::type;

  template <typename Interface, typename KeyDomain, typename Cardinality>
  struct ordinary_runtime_binding_slot;

  template <typename Interface, typename Cardinality>
  struct ordinary_runtime_binding_slot<Interface, ::dingo::no_key,
                                       Cardinality> {
    using type = no_key_runtime_binding_slot<Interface, Cardinality>;
  };

  template <typename Interface, typename Key, typename Cardinality>
  struct ordinary_runtime_binding_slot<Interface, ::dingo::typed_key<Key>,
                                       Cardinality> {
    using type = typed_key_runtime_binding_slot<Interface, Key, Cardinality>;
  };

  template <typename Interface, typename KeyDomain, typename Cardinality>
  using ordinary_runtime_binding_slot_t =
      typename ordinary_runtime_binding_slot<Interface, KeyDomain,
                                             Cardinality>::type;

  template <typename Interface, typename KeyDomain, typename Cardinality>
  runtime_type_bindings *
  ordinary_slot_bindings_at(runtime_bindings_state *state) {
    if (!state) {
      return nullptr;
    }
    using slot =
        ordinary_runtime_binding_slot_t<Interface, KeyDomain, Cardinality>;
    return state->type_bindings.template get<slot>();
  }

  template <typename Interface, typename Key>
  using runtime_key_lookup_entry_t =
      detail::selected_lookup_entry_t<Interface, Key, normalized_view_entries>;

  template <typename Interface, typename Key>
  using runtime_key_lookup_cardinality_t = typename lookup_entry_cardinality<
      runtime_key_lookup_entry_t<Interface, Key>>::type;

  template <typename Interface>
  static constexpr bool uses_no_key_singular_fast_path() {
    return !has_explicit_no_key_many_lookup_v<Interface>;
  }

  template <typename T, typename IdType>
  static constexpr bool has_explicit_collection_lookup(IdType &&) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    if constexpr (is_none_v<std::decay_t<IdType>>) {
      return has_explicit_no_key_many_lookup_v<index_interface_type>;
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      using key_type = typename std::decay_t<IdType>::type;
      return std::is_same_v<
          typed_key_lookup_cardinality_t<index_interface_type, key_type>,
          ::dingo::many>;
    } else {
      using index_key_type = std::decay_t<IdType>;
      using lookup_entry =
          runtime_key_lookup_entry_t<index_interface_type, index_key_type>;
      return !std::is_void_v<lookup_entry> &&
             std::is_same_v<runtime_key_lookup_cardinality_t<
                                index_interface_type, index_key_type>,
                            ::dingo::many>;
    }
  }

  template <typename T, bool CheckCache, typename IdType>
  struct selected_runtime_binding {
    registry_type &registry;
    IdType &id;

    decltype(auto) select() {
      return registry.template runtime_source_select<T>(
          std::forward<IdType>(id));
    }

    template <typename Request, typename Selection>
    decltype(auto) resolve(runtime_context &context, Selection selection) {
      return registry.template runtime_source_resolve<T, CheckCache>(
          selection, context, std::forward<IdType>(id));
    }
  };

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType>
  struct missing_runtime_binding {
    registry_type &registry;
    IdType &id;

    template <typename Request>
    request_interface_t<Request> resolve(runtime_context &context) {
      return registry.template runtime_source_missing<
          T, RemoveRvalueReferences, MayAutoConstruct, IdType,
          request_interface_t<Request>>(context, std::forward<IdType>(id));
    }
  };

  template <typename Request, typename IdType, typename Interface>
  runtime_type_bindings *
  runtime_source_bindings_at_interface(runtime_bindings_state *state) {
    if (!state) {
      return nullptr;
    }

    if constexpr (is_none_v<std::decay_t<IdType>>) {
      using lookup_cardinality = no_key_lookup_cardinality_t<Interface>;
      return ordinary_slot_bindings_at<Interface, ::dingo::no_key,
                                       lookup_cardinality>(state);
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      using key_id_type = std::decay_t<IdType>;
      using key_type = typename key_id_type::type;
      using lookup_cardinality =
          typed_key_lookup_cardinality_t<Interface, key_type>;
      return ordinary_slot_bindings_at<Interface, ::dingo::typed_key<key_type>,
                                       lookup_cardinality>(state);
    } else {
      using key_type = std::decay_t<IdType>;
      using lookup_entry = runtime_key_lookup_entry_t<Interface, key_type>;
      if constexpr (std::is_void_v<lookup_entry>) {
        (void)state;
        return nullptr;
      } else {
        using slot = runtime_key_runtime_binding_slot<
            Interface, key_type,
            runtime_key_lookup_cardinality_t<Interface, key_type>>;
        return state->type_bindings.template get<slot>();
      }
    }
  }

  template <typename Request, typename IdType>
  runtime_type_bindings *
  runtime_source_bindings(runtime_bindings_state *state) {
    using lookup_type = normalized_type_t<Request>;
    using exact_type =
        std::remove_cv_t<std::remove_reference_t<request_interface_t<Request>>>;
    auto *data = [&]() {
      if constexpr (!is_none_v<std::decay_t<IdType>> &&
                    !detail::is_typed_key_v<IdType>) {
        return runtime_source_bindings_at_interface<Request, IdType,
                                                    lookup_type>(state);
      } else {
        return runtime_source_bindings_at_interface<Request, IdType,
                                                    exact_type>(state);
      }
    }();
    if constexpr (!std::is_same_v<lookup_type, exact_type>) {
      if (!data) {
        if constexpr (!is_none_v<std::decay_t<IdType>> &&
                      !detail::is_typed_key_v<IdType>) {
          data =
              runtime_source_bindings_at_interface<Request, IdType, exact_type>(
                  state);
        } else {
          data = runtime_source_bindings_at_interface<Request, IdType,
                                                      lookup_type>(state);
        }
      }
    }
    return data;
  }

  template <typename Request, typename IdType = none_t>
  runtime_selection runtime_source_select(IdType &&id = IdType()) {
    auto *state = runtime_bindings_if_present();
    auto *data = runtime_source_bindings<Request, IdType>(state);
    return select_runtime_binding<Request>(data, std::forward<IdType>(id));
  }

  template <typename T, bool CheckCache, typename IdType>
  auto runtime_source_resolve(runtime_selection selection,
                              runtime_context &context, IdType &&id)
      -> request_interface_t<T> {
    (void)id;
    if constexpr (is_none_v<std::decay_t<IdType>>) {
      return resolve<T, request_interface_t<T>>(*selection.binding, context);
    } else {
      if constexpr (cache_enabled && CheckCache) {
        if (selection.state->cache) {
          return detail::convert_resolved_binding<request_interface_t<T>>(
              selection.state->cache);
        }
      }

      return resolve<T, request_interface_t<T>>(*selection.binding, context,
                                                *selection.state);
    }
  }

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R runtime_source_missing(runtime_context &context, IdType &&id) {
    using Type = normalized_type_t<T>;
    (void)id;

    if constexpr (!std::is_same_v<void, ParentRegistry> &&
                  !std::is_same_v<void *, decltype(resolve_root_)> &&
                  !std::is_base_of_v<registry_type, resolve_root_type>) {
      if (resolve_root_) {
        if constexpr (is_none_v<std::decay_t<IdType>>) {
          return resolve_root()
              ->template resolve<T, RemoveRvalueReferences, true>(context,
                                                                  none_t{});
        } else if constexpr (detail::is_typed_key_v<IdType>) {
          return resolve_root()
              ->template resolve<T, RemoveRvalueReferences, true>(
                  context, std::decay_t<IdType>{});
        } else {
          return resolve_root()->template resolve<T>(std::forward<IdType>(id));
        }
      }
    }

    if constexpr (MayAutoConstruct && detail::is_typed_key_v<IdType> &&
                  collection_traits<R>::is_collection) {
      return this->template construct_collection_runtime_request<R>(
          detail::binding_collection_append{}, std::decay_t<IdType>{});
    } else if constexpr (MayAutoConstruct &&
                         is_auto_constructible<std::decay_t<T>>::value) {
      if constexpr (constructor<Type>::kind ==
                    detail::constructor_kind::concrete) {
        return auto_construct<T>(context);
      } else if constexpr (is_none_v<std::decay_t<IdType>>) {
        throw detail::make_type_not_found_exception<T>(context);
      } else {
        throw detail::make_type_not_found_exception<T, std::decay_t<IdType>>(
            context);
      }
    } else if constexpr (is_none_v<std::decay_t<IdType>>) {
      throw detail::make_type_not_found_exception<T>(context);
    } else {
      throw detail::make_type_not_found_exception<T, std::decay_t<IdType>>(
          context);
    }
  }

  template <typename T, typename IdType>
  runtime_type_bindings *runtime_collection_bindings(IdType &&) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    auto *state = runtime_bindings_if_present();
    return runtime_source_bindings<resolve_type, IdType>(state);
  }

  template <typename T, typename Fn>
  std::size_t append_no_key_one_collection(runtime_type_bindings &data,
                                           T &results, runtime_context &context,
                                           Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    std::size_t inline_count = 0;
    if (auto *entry = data.no_key_singular_entry()) {
      fn(results,
         resolve_collection_type<resolve_type>(*entry->binding, context));
      inline_count = 1;
    }

    return inline_count +
           data.template for_each_no_key<index_interface_type, ::dingo::one>(
               [&](auto &entry) {
                 fn(results, resolve_collection_type<resolve_type>(
                                 *entry.binding, context));
               });
  }

  template <typename T, typename Key, typename Fn>
  std::size_t
  append_typed_key_one_collection(runtime_type_bindings &data, T &results,
                                  runtime_context &context, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    return data.template for_each_typed_key<index_interface_type, Key,
                                            ::dingo::one>([&](auto &entry) {
      fn(results,
         resolve_collection_type<resolve_type>(*entry.binding, context));
    });
  }

  template <typename T, typename Fn, typename IdType>
  std::size_t append_runtime_keyed_collection(runtime_type_bindings &data,
                                              T &results,
                                              runtime_context &context, Fn &&fn,
                                              IdType id) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_key_type = std::decay_t<IdType>;
    using index_interface_type = normalized_type_t<resolve_type>;
    using lookup_entry =
        runtime_key_lookup_entry_t<index_interface_type, index_key_type>;

    if constexpr (std::is_void_v<lookup_entry>) {
      (void)results;
      (void)context;
      (void)fn;
      return 0;
    } else {
      if constexpr (std::is_same_v<runtime_key_lookup_cardinality_t<
                                       index_interface_type, index_key_type>,
                                   ::dingo::many>) {
        return data.template for_each_runtime_key<
            index_interface_type, index_key_type, ::dingo::many, lookup_entry>(
            id, [&](auto &entry) {
              fn(results, resolve_collection_type<resolve_type>(*entry.binding,
                                                                context));
            });
      } else {
        (void)results;
        (void)context;
        (void)fn;
        return 0;
      }
    }
  }

  template <typename T, typename Fn>
  std::size_t
  append_no_key_runtime_collection(runtime_type_bindings &data, T &results,
                                   runtime_context &context, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    if constexpr (!has_explicit_no_key_many_lookup_v<index_interface_type>) {
      if constexpr (!has_explicit_no_key_one_lookup_v<index_interface_type>) {
        return append_no_key_one_collection(data, results, context,
                                            std::forward<Fn>(fn));
      } else {
        (void)results;
        (void)context;
        (void)fn;
        return 0;
      }
    } else {
      return data.template for_each_no_key<index_interface_type, ::dingo::many>(
          [&](auto &entry) {
            fn(results,
               resolve_collection_type<resolve_type>(*entry.binding, context));
          });
    }
  }

  template <typename T, typename Fn, typename IdType>
  std::size_t append_typed_key_runtime_collection(runtime_type_bindings &data,
                                                  T &results,
                                                  runtime_context &context,
                                                  Fn &&fn, IdType) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using key_id_type = std::decay_t<IdType>;
    using key_type = typename key_id_type::type;
    using index_interface_type = normalized_type_t<resolve_type>;

    if constexpr (!std::is_same_v<typed_key_lookup_cardinality_t<
                                      index_interface_type, key_type>,
                                  ::dingo::many>) {
      if constexpr (!has_explicit_typed_key_one_lookup_v<index_interface_type,
                                                         key_type>) {
        return append_typed_key_one_collection<T, key_type>(
            data, results, context, std::forward<Fn>(fn));
      } else {
        (void)results;
        (void)context;
        (void)fn;
        return 0;
      }
    } else {
      return data.template for_each_typed_key<index_interface_type, key_type,
                                              ::dingo::many>([&](auto &entry) {
        fn(results,
           resolve_collection_type<resolve_type>(*entry.binding, context));
      });
    }
  }

  template <typename T>
  std::size_t count_no_key_one_collection(runtime_type_bindings &data) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    return (data.has_no_key_singular_entry() ? 1U : 0U) +
           data.template count_no_key<index_interface_type, ::dingo::one>();
  }

  template <typename T, typename Key>
  std::size_t count_typed_key_one_collection(runtime_type_bindings &data) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    return data
        .template count_typed_key<index_interface_type, Key, ::dingo::one>();
  }

  template <typename T, typename IdType>
  std::size_t count_runtime_keyed_collection(runtime_type_bindings &data,
                                             IdType id) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_key_type = std::decay_t<IdType>;
    using index_interface_type = normalized_type_t<resolve_type>;
    using lookup_entry =
        runtime_key_lookup_entry_t<index_interface_type, index_key_type>;

    if constexpr (std::is_void_v<lookup_entry>) {
      return 0;
    } else {
      if constexpr (std::is_same_v<runtime_key_lookup_cardinality_t<
                                       index_interface_type, index_key_type>,
                                   ::dingo::many>) {
        return data.template count_runtime_key<
            index_interface_type, index_key_type, ::dingo::many, lookup_entry>(
            id);
      } else {
        return 0;
      }
    }
  }

  template <typename T>
  std::size_t count_no_key_runtime_collection(runtime_type_bindings &data) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    if constexpr (!has_explicit_no_key_many_lookup_v<index_interface_type>) {
      if constexpr (!has_explicit_no_key_one_lookup_v<index_interface_type>) {
        return count_no_key_one_collection<T>(data);
      } else {
        return 0;
      }
    } else {
      return data.template count_no_key<index_interface_type, ::dingo::many>();
    }
  }

  template <typename T, typename IdType>
  std::size_t count_typed_key_runtime_collection(runtime_type_bindings &data,
                                                 IdType) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using key_id_type = std::decay_t<IdType>;
    using key_type = typename key_id_type::type;
    using index_interface_type = normalized_type_t<resolve_type>;

    if constexpr (!std::is_same_v<typed_key_lookup_cardinality_t<
                                      index_interface_type, key_type>,
                                  ::dingo::many>) {
      if constexpr (!has_explicit_typed_key_one_lookup_v<index_interface_type,
                                                         key_type>) {
        return count_typed_key_one_collection<T, key_type>(data);
      } else {
        return 0;
      }
    } else {
      return data.template count_typed_key<index_interface_type, key_type,
                                           ::dingo::many>();
    }
  }

  template <typename T, typename Fn, typename IdType>
  std::size_t append_runtime_collection(T &results, runtime_context &context,
                                        Fn &&fn, IdType id) {
    if constexpr (!is_none_v<std::decay_t<IdType>> &&
                  !detail::is_typed_key_v<IdType>) {
      auto *data = runtime_collection_bindings<T>(id);
      return data ? append_runtime_keyed_collection(*data, results, context,
                                                    std::forward<Fn>(fn), id)
                  : 0;
    } else if constexpr (is_none_v<std::decay_t<IdType>>) {
      auto *data = runtime_collection_bindings<T>(id);
      return data ? append_no_key_runtime_collection(*data, results, context,
                                                     std::forward<Fn>(fn))
                  : 0;
    } else {
      auto *data = runtime_collection_bindings<T>(id);
      return data ? append_typed_key_runtime_collection(
                        *data, results, context, std::forward<Fn>(fn), id)
                  : 0;
    }
  }

  template <typename T, typename IdType>
  std::size_t count_runtime_collection(IdType id) {
    if constexpr (!is_none_v<std::decay_t<IdType>> &&
                  !detail::is_typed_key_v<IdType>) {
      auto *data = runtime_collection_bindings<T>(id);
      return data ? count_runtime_keyed_collection<T>(*data, id) : 0;
    } else if constexpr (is_none_v<std::decay_t<IdType>>) {
      auto *data = runtime_collection_bindings<T>(id);
      return data ? count_no_key_runtime_collection<T>(*data) : 0;
    } else {
      auto *data = runtime_collection_bindings<T>(id);
      return data ? count_typed_key_runtime_collection<T>(*data, id) : 0;
    }
  }

private:
  template <typename Request, typename IdType>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  runtime_selection select_runtime_binding(runtime_type_bindings *data,
                                           IdType &&id) {
    if constexpr (is_none_v<std::decay_t<IdType>>) {
      using index_interface_type = normalized_type_t<Request>;
      return select_no_key_lookup_table_binding<
          index_interface_type,
          no_key_lookup_cardinality_t<index_interface_type>>(data);
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      using key_id_type = std::decay_t<IdType>;
      using key_type = typename key_id_type::type;
      using index_interface_type = normalized_type_t<Request>;
      return select_typed_key_lookup_table_binding<
          index_interface_type, key_type,
          typed_key_lookup_cardinality_t<index_interface_type, key_type>>(data);
    } else {
      using index_key_type = std::decay_t<IdType>;
      using index_interface_type = normalized_type_t<Request>;
      using lookup_entry =
          runtime_key_lookup_entry_t<index_interface_type, index_key_type>;
      if constexpr (std::is_void_v<lookup_entry>) {
        if constexpr (!std::is_same_v<void, ParentRegistry> &&
                      !std::is_base_of_v<registry_type, resolve_root_type>) {
          return detail::make_runtime_selection<runtime_binding_interface_type,
                                                binding_cache_state *>(nullptr,
                                                                       nullptr);
        } else {
          static_assert(!std::is_void_v<lookup_entry>,
                        "keyed registration or request has no matching "
                        "dingo view definition for interface/key");
        }
      } else {
        return select_runtime_key_lookup_table_binding<
            index_interface_type, index_key_type,
            runtime_key_lookup_cardinality_t<index_interface_type,
                                             index_key_type>,
            lookup_entry>(data, id);
      }
    }
  }

  template <typename Interface, typename Cardinality>
  runtime_selection
  select_no_key_lookup_table_binding(runtime_type_bindings *data) {
    registered_binding_entry *selected = nullptr;
    bool ambiguous = false;
    if (data) {
      if constexpr (std::is_same_v<Cardinality, ::dingo::one>) {
        selected = data->no_key_singular_entry();
      }
      if (!selected) {
        selected = data->template find_singular_no_key<Interface, Cardinality>(
            ambiguous);
      }
    }

    return make_lookup_table_selection(selected, ambiguous);
  }

  template <typename Interface, typename Key, typename Cardinality>
  runtime_selection
  select_typed_key_lookup_table_binding(runtime_type_bindings *data) {
    registered_binding_entry *selected = nullptr;
    bool ambiguous = false;
    if (data) {
      selected =
          data->template find_singular_typed_key<Interface, Key, Cardinality>(
              ambiguous);
    }

    return make_lookup_table_selection(selected, ambiguous);
  }

  template <typename Interface, typename Key, typename Cardinality,
            typename LookupEntry, typename KeyValue>
  runtime_selection
  select_runtime_key_lookup_table_binding(runtime_type_bindings *data,
                                          const KeyValue &key_value) {
    registered_binding_entry *selected = nullptr;
    bool ambiguous = false;
    if (data) {
      selected =
          data->template find_singular_runtime_key<Interface, Key, Cardinality,
                                                   LookupEntry>(key_value,
                                                                ambiguous);
    }

    return make_lookup_table_selection(selected, ambiguous);
  }

  runtime_selection
  make_lookup_table_selection(registered_binding_entry *selected,
                              bool ambiguous) {
    if (ambiguous) {
      return runtime_selection::ambiguity();
    }
    return detail::make_runtime_selection<runtime_binding_interface_type,
                                          binding_cache_state *>(
        selected ? selected->binding.get() : nullptr, selected);
  }

  template <typename... TypeArgs, typename Parent, typename Arg,
            typename IdType>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  auto &register_type_impl(Parent *parent, Arg &&arg, IdType &&id) {
    static_assert(!detail::has_explicit_void_interface_v<TypeArgs...>,
                  "interfaces<void> is not a valid registration target");
    using registration =
        std::conditional_t<!is_none_v<std::decay_t<Arg>>,
                           type_registration<TypeArgs..., factory<Arg>>,
                           type_registration<TypeArgs...>>;
    static_assert(!detail::is_key_value_v<typename registration::key_type>,
                  "dingo::key<T, V> registration keys require a static fixed "
                  "runtime-key request");
    using binding_model = detail::binding_model<registration>;
    using bindings_type = typename binding_model::bindings_type;
    using instance_container_type = registration_container_type<registration>;
    using resolution_container_type = std::conditional_t<
        std::is_void_v<bindings_type>, instance_container_type,
        detail::binding_resolution<resolve_root_type, bindings_type>>;
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
      using key_id_type =
          std::conditional_t<std::is_void_v<typename binding_model::key_type>,
                             none_t, key<typename binding_model::key_type>>;

      registration_requirements::assert_valid();

      using runtime_binding_state_type =
          runtime_binding_state<instance_container_type, storage_type,
                                resolution_container_type>;

      if constexpr (registration_requirements::valid &&
                    type_list_size_v<interface_types> == 1) {
        using interface_type = type_list_head_t<interface_types>;

        using runtime_binding_type =
            runtime_binding<container_type,
                            typename annotated_traits<interface_type>::type,
                            storage_type, runtime_binding_state_type>;
        using registered_binding_type = std::conditional_t<
            is_none_v<key_id_type>, runtime_binding_type,
            detail::keyed_binding_identity<key_id_type, runtime_binding_type>>;

        if constexpr (is_none_v<std::decay_t<IdType>> &&
                      is_none_v<key_id_type> &&
                      uses_no_key_singular_fast_path<interface_type>()) {
          if constexpr (!is_none_v<std::decay_t<Arg>>) {
            return register_no_key_singular_binding<
                interface_type, storage_type, registered_binding_type>(
                parent, std::forward<Arg>(arg));
          } else {
            return register_no_key_singular_binding<
                interface_type, storage_type, registered_binding_type>(parent);
          }
        } else if constexpr (!is_none_v<std::decay_t<Arg>>) {
          return register_constructed_type_binding<interface_type, storage_type,
                                                   registered_binding_type>(
              parent, std::forward<IdType>(id), key_id_type{},
              std::forward<Arg>(arg));
        } else {
          return register_constructed_type_binding<interface_type, storage_type,
                                                   registered_binding_type>(
              parent, std::forward<IdType>(id), key_id_type{});
        }
      } else {
        if constexpr (registration_requirements::valid) {
          std::shared_ptr<runtime_binding_state_type> data;
          if constexpr (!is_none_v<std::decay_t<Arg>>) {
            data = std::allocate_shared<runtime_binding_state_type>(
                allocator_traits::rebind<runtime_binding_state_type>(
                    get_allocator()),
                parent, std::forward<Arg>(arg));
          } else {
            data = std::allocate_shared<runtime_binding_state_type>(
                allocator_traits::rebind<runtime_binding_state_type>(
                    get_allocator()),
                parent);
          }

          for_each(interface_types{}, [&](auto element) {
            using interface_type = typename decltype(element)::type;

            using runtime_binding_type = runtime_binding<
                container_type, typename annotated_traits<interface_type>::type,
                storage_type, std::shared_ptr<runtime_binding_state_type>>;
            using registered_binding_type =
                std::conditional_t<is_none_v<key_id_type>, runtime_binding_type,
                                   detail::keyed_binding_identity<
                                       key_id_type, runtime_binding_type>>;

            register_type_binding<interface_type, storage_type>(
                allocate_binding<registered_binding_type>(data).first, id,
                key_id_type{});
          });
          return data->instance_container_ref();
        } else {
          return invalid_registration_return<instance_container_type>();
        }
      }
    } else {
      return invalid_registration_return<instance_container_type>();
    }
  }

  template <typename TypeInterface, typename TypeStorage>
  void reject_existing_no_key_singular(runtime_type_bindings &data) {
    if (data.has_no_key_singular_entry()) {
      throw detail::make_type_already_registered_exception<
          TypeInterface, typename TypeStorage::type>();
    }
  }

  template <typename Slot, typename Cardinality, typename LookupEntry>
  runtime_type_bindings &ensure_ordinary_slot_bindings() {
    static_assert(detail::is_ordinary_selector_lookup_entry_v<LookupEntry>,
                  "ordinary slot bindings require an ordinary no_key or "
                  "typed_key LookupEntry");
    static_assert(
        std::is_same_v<typename lookup_entry_cardinality<LookupEntry>::type,
                       Cardinality>,
        "ordinary slot LookupEntry cardinality must match slot cardinality");
    return ensure_runtime_bindings()
        .type_bindings
        .template insert<Slot>(
            get_allocator(),
            typename runtime_type_bindings::template ordinary_slot_tag_t<
                Cardinality, LookupEntry>{})
        .first;
  }

  template <typename Interface, typename KeyDomain, typename Cardinality>
  runtime_type_bindings &ensure_ordinary_slot_at() {
    using lookup_entry =
        selected_ordinary_lookup_entry_t<Interface, KeyDomain, Cardinality>;
    using slot =
        ordinary_runtime_binding_slot_t<Interface, KeyDomain, Cardinality>;
    return ensure_ordinary_slot_bindings<slot, Cardinality, lookup_entry>();
  }

  template <typename TypeInterface, typename TypeStorage,
            typename ExceptionFactory>
  void commit_registration_entry(
      runtime_type_bindings &data,
      typename runtime_type_bindings::entry_handle inserted_entry,
      ExceptionFactory &&make_exception) {
    bool binding_registered = true;
    bool lookup_rows_registered = false;
    try {
      if (!data.insert_lookup_rows(inserted_entry)) {
        data.erase_entry(inserted_entry);
        binding_registered = false;
        throw make_exception();
      }
      lookup_rows_registered = true;
    } catch (...) {
      if (binding_registered) {
        if (lookup_rows_registered) {
          data.erase_lookup_rows(std::addressof(*inserted_entry));
        }
        data.erase_entry(inserted_entry);
      }
      throw;
    }
  }

  template <typename Result> struct registered_binding_insertion {
    typename runtime_type_bindings::entry_handle inserted_entry;
    Result result;
  };

  template <typename TypeInterface, typename TypeStorage, typename EntryFactory,
            typename ExceptionFactory>
  auto register_slot_binding_transaction(
      runtime_type_bindings &data,
      typename rtti_type::type_index resolved_storage_type,
      std::optional<typename rtti_type::type_index> resolved_key_type,
      lookup_identity &&lookup_id, EntryFactory &&entry_factory,
      ExceptionFactory &&make_exception) {
    auto insertion =
        entry_factory(data, std::move(resolved_storage_type),
                      std::move(resolved_key_type), std::move(lookup_id));
    commit_registration_entry<TypeInterface, TypeStorage>(
        data, insertion.inserted_entry,
        std::forward<ExceptionFactory>(make_exception));
    runtime_bindings_present_ = true;
    return insertion;
  }

  template <typename TypeInterface, typename TypeStorage, typename IdType,
            typename KeyIdType>
  static auto make_registration_duplicate_exception() {
    if constexpr (!is_none_v<std::decay_t<IdType>>) {
      return detail::make_type_index_already_registered_exception<
          TypeInterface, typename TypeStorage::type, IdType>();
    } else if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
      return detail::make_type_already_registered_exception<
          TypeInterface, typename TypeStorage::type>();
    } else {
      return detail::make_type_index_already_registered_exception<
          TypeInterface, typename TypeStorage::type, std::decay_t<KeyIdType>>();
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename KeyDomain,
            typename Cardinality, typename IdType, typename KeyIdType,
            typename EntryFactory>
  auto register_ordinary_slot_binding(
      typename rtti_type::type_index resolved_storage_type,
      std::optional<typename rtti_type::type_index> resolved_key_type,
      lookup_identity &&lookup_id, EntryFactory &&entry_factory) {
    auto &data =
        ensure_ordinary_slot_at<TypeInterface, KeyDomain, Cardinality>();
    if constexpr (std::is_same_v<KeyDomain, ::dingo::no_key> &&
                  std::is_same_v<Cardinality, ::dingo::one>) {
      reject_existing_no_key_singular<TypeInterface, TypeStorage>(data);
    }
    return register_slot_binding_transaction<TypeInterface, TypeStorage>(
        data, std::move(resolved_storage_type), std::move(resolved_key_type),
        std::move(lookup_id), std::forward<EntryFactory>(entry_factory), [] {
          return make_registration_duplicate_exception<
              TypeInterface, TypeStorage, IdType, KeyIdType>();
        });
  }

  template <typename TypeInterface, typename TypeStorage, typename IdType,
            typename KeyIdType, typename EntryFactory>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  decltype(auto) register_routed_type_binding(IdType &&id, KeyIdType,
                                              EntryFactory &&entry_factory) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();

    auto resolved_storage_type =
        rtti_type::template get_type_index<typename TypeStorage::type>();

    if constexpr (!is_none_v<std::decay_t<IdType>>) {
      using lookup_entry = runtime_key_lookup_entry_t<TypeInterface, IdType>;
      static_assert(!std::is_void_v<lookup_entry>,
                    "keyed registration or request has no matching "
                    "dingo view definition for interface/key");
      using lookup_cardinality_type =
          runtime_key_lookup_cardinality_t<TypeInterface, IdType>;
      using slot = runtime_key_runtime_binding_slot<TypeInterface, IdType,
                                                    lookup_cardinality_type>;
      auto &data =
          ensure_runtime_bindings()
              .type_bindings
              .template insert<slot>(
                  get_allocator(),
                  typename runtime_type_bindings::runtime_key_slot_tag{})
              .first;
      auto resolved_key_type = [&]() {
        if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
          return std::optional<typename rtti_type::type_index>{};
        } else {
          return std::optional<typename rtti_type::type_index>{
              rtti_type::template get_type_index<std::decay_t<KeyIdType>>()};
        }
      }();
      auto lookup_id = lookup_table_type::template make_runtime_key_identity<
          TypeInterface, IdType, lookup_cardinality_type>(get_allocator(), id);
      return register_slot_binding_transaction<TypeInterface, TypeStorage>(
          data, std::move(resolved_storage_type), std::move(resolved_key_type),
          std::move(lookup_id), std::forward<EntryFactory>(entry_factory), [] {
            return make_registration_duplicate_exception<
                TypeInterface, TypeStorage, IdType, KeyIdType>();
          });
    } else if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
      using lookup_cardinality_type =
          no_key_lookup_cardinality_t<TypeInterface>;
      auto lookup_id = lookup_table_type::template make_no_key_identity<
          TypeInterface, lookup_cardinality_type>();
      return register_ordinary_slot_binding<
          TypeInterface, TypeStorage, ::dingo::no_key, lookup_cardinality_type,
          IdType, KeyIdType>(std::move(resolved_storage_type),
                             std::optional<typename rtti_type::type_index>{},
                             std::move(lookup_id),
                             std::forward<EntryFactory>(entry_factory));
    } else {
      using typed_key_type = typename std::decay_t<KeyIdType>::type;
      using lookup_cardinality_type =
          typed_key_lookup_cardinality_t<TypeInterface, typed_key_type>;
      auto lookup_id = lookup_table_type::template make_typed_key_identity<
          TypeInterface, typed_key_type, lookup_cardinality_type>();
      return register_ordinary_slot_binding<
          TypeInterface, TypeStorage, ::dingo::typed_key<typed_key_type>,
          lookup_cardinality_type, IdType, KeyIdType>(
          std::move(resolved_storage_type),
          std::optional<typename rtti_type::type_index>{
              rtti_type::template get_type_index<std::decay_t<KeyIdType>>()},
          std::move(lookup_id), std::forward<EntryFactory>(entry_factory));
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename Binding,
            typename IdType, typename KeyIdType>
  void register_type_binding(Binding &&binding, IdType &&id, KeyIdType) {
    auto insertion = register_routed_type_binding<TypeInterface, TypeStorage,
                                                  IdType, KeyIdType>(
        std::forward<IdType>(id), KeyIdType{},
        [&](auto &data, auto &&resolved_storage_type, auto &&resolved_key_type,
            auto &&lookup_id) {
          return registered_binding_insertion<none_t>{
              data.emplace_entry(
                  std::forward<Binding>(binding),
                  std::forward<decltype(resolved_storage_type)>(
                      resolved_storage_type),
                  std::forward<decltype(resolved_key_type)>(resolved_key_type),
                  std::forward<decltype(lookup_id)>(lookup_id)),
              none_t{}};
        });
    (void)insertion;
  }

  template <typename TypeInterface, typename TypeStorage, typename Binding,
            typename Parent, typename IdType, typename KeyIdType,
            typename... Args>
  typename Binding::container_type &
  register_constructed_type_binding(Parent *parent, IdType &&id, KeyIdType,
                                    Args &&...args) {
    auto insertion = register_routed_type_binding<TypeInterface, TypeStorage,
                                                  IdType, KeyIdType>(
        std::forward<IdType>(id), KeyIdType{},
        [&](auto &data, auto &&resolved_storage_type, auto &&resolved_key_type,
            auto &&lookup_id) {
          auto &&[inserted_entry, binding_container] =
              data.template emplace_inline_binding_entry<Binding>(
                  std::forward<decltype(resolved_storage_type)>(
                      resolved_storage_type),
                  std::forward<decltype(resolved_key_type)>(resolved_key_type),
                  std::forward<decltype(lookup_id)>(lookup_id), parent,
                  std::forward<Args>(args)...);
          return registered_binding_insertion<
              typename Binding::container_type *>{inserted_entry,
                                                  binding_container};
        });
    return *insertion.result;
  }

  template <typename TypeInterface, typename TypeStorage, typename Binding,
            typename... Args>
  typename Binding::container_type &
  register_no_key_singular_binding(Args &&...args) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();

    auto &state = ensure_runtime_bindings();
    using slot = no_key_runtime_binding_slot<TypeInterface, ::dingo::one>;
    auto pb = state.type_bindings.template insert<slot>(
        get_allocator(), typename runtime_type_bindings::no_key_slot_tag{});
    auto &data = pb.first;

    bool type_bindings_inserted = pb.second;
    try {
      bool ambiguous = false;
      auto *selected =
          data.template find_singular_no_key<TypeInterface, ::dingo::one>(
              ambiguous);
      if (data.has_no_key_singular_entry() || selected || ambiguous) {
        throw detail::make_type_already_registered_exception<
            TypeInterface, typename TypeStorage::type>();
      }

      auto resolved_storage_type =
          rtti_type::template get_type_index<typename TypeStorage::type>();
      auto lookup_id =
          lookup_table_type::template make_no_key_identity<TypeInterface,
                                                           ::dingo::one>();
      auto &binding_container =
          data.template emplace_no_key_singular_entry<Binding>(
              std::move(resolved_storage_type), std::move(lookup_id),
              std::forward<Args>(args)...);
      runtime_bindings_present_ = true;
      return binding_container;
    } catch (...) {
      if (type_bindings_inserted) {
        state.type_bindings.template erase<slot>();
      }
      throw;
    }
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            bool CheckCache = true, typename IdType = none_t,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve_impl(runtime_context &context, IdType &&id = IdType()) {
    using Type = normalized_type_t<T>;
    static_assert(!std::is_const_v<Type>);

    if constexpr (cache_enabled && CheckCache) {
      if (auto *state = runtime_bindings_if_present()) {
        void *cache = state->type_cache.template get<T>();
        if (cache) {
          return detail::convert_resolved_binding<request_interface_t<T>>(
              cache);
        }
      }
    }

    selected_runtime_binding<T, CheckCache, IdType> selected{*this, id};
    missing_runtime_binding<T, RemoveRvalueReferences, MayAutoConstruct, IdType>
        missing{*this, id};
    auto sources = detail::make_selected_binding_sources(selected, missing);
    return detail::resolve_from_binding_sources<T, R>(context, sources);
  }

  template <typename T>
  decltype(auto) auto_construct(runtime_context &context) {
    using Type = normalized_type_t<T>;

    static_assert(is_complete<Type>::value,
                  "auto-construction requires a complete type");

    using type_detection = detail::automatic;
    return context
        .template construct_temporary<request_interface_t<T>, type_detection>(
            *resolve_root());
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
            typename IdType = none_t,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, IdType &&id = IdType()) {
    return resolve_impl < T, RemoveRvalueReferences,
           std::is_same_v<request_value_t<T>, std::decay_t<T>> &&
               (!std::is_reference_v<T> ||
                (std::is_lvalue_reference_v<T> &&
                 std::is_const_v<std::remove_reference_t<T>> &&
                 is_auto_constructible<std::decay_t<T>>::value)),
           CheckCache > (context, std::forward<IdType>(id));
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  template <typename CachedT, typename T, typename Binding, typename Context>
  T resolve(Binding &binding, Context &context) {
    return ::dingo::resolve_binding_request<T, rtti_type>(
        binding, context,
        cache_enabled
            ? instance_cache_sink{this,
                                  &registry_type::store_type_cache<CachedT>}
            : instance_cache_sink{});
  }

  template <typename CachedT, typename T, typename Binding, typename Context>
  T resolve(Binding &binding, Context &context, binding_cache_state &data) {
    return ::dingo::resolve_binding_request<T, rtti_type>(
        binding, context,
        cache_enabled
            ? instance_cache_sink{&data, &registry_type::store_index_cache}
            : instance_cache_sink{});
  }

  template <typename T, typename Binding, typename Context>
  T resolve_collection_type(Binding &binding, Context &context) {
    return ::dingo::resolve_binding_request<T, rtti_type>(binding, context);
  }

  template <class Storage, class TypeInterface, class Type>
  void check_interface_requirements() {
    detail::interface_registration_requirements<Storage, TypeInterface,
                                                Type>::assert_valid();
  }

  template <typename U, typename... Args>
  std::pair<runtime_binding_ptr<runtime_binding_interface<container_type>>,
            typename U::container_type *>
  allocate_binding(Args &&...args) {
    auto alloc = allocator_traits::rebind<U>(get_allocator());
    U *instance = allocator_traits::allocate(alloc, 1);
    if (!instance)
      return {nullptr, nullptr};

    allocator_traits::construct(alloc, instance, std::forward<Args>(args)...);

    return std::make_pair(
        runtime_binding_ptr<runtime_binding_interface<container_type>>(
            instance, &registry_type::template destroy_binding<U>),
        &instance->get_container());
  }

  template <typename U>
  static void destroy_binding(runtime_binding_interface<container_type> *ptr) {
    auto *instance = static_cast<U *>(ptr);
    auto alloc =
        allocator_traits::rebind<U>(instance->get_container().get_allocator());
    allocator_traits::destroy(alloc, instance);
    allocator_traits::deallocate(alloc, instance, 1);
  }

  template <typename U>
  static void
  destroy_inline_binding(runtime_binding_interface<container_type> *ptr) {
    auto *instance = static_cast<U *>(ptr);
    instance->~U();
  }

  resolve_root_type *resolve_root_ = nullptr;

  runtime_bindings_state_storage runtime_bindings_;

  bool runtime_bindings_present_ = false;
};

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
auto runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::runtime_bindings_if_present()
    -> runtime_bindings_state * {
  return runtime_bindings_.get();
}

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
auto runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::runtime_bindings_if_present() const
    -> const runtime_bindings_state * {
  return runtime_bindings_.get();
}

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
auto runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::ensure_runtime_bindings()
    -> runtime_bindings_state & {
  return runtime_bindings_.ensure(get_allocator());
}

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
void runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::destroy_runtime_bindings() {
  runtime_bindings_.destroy(get_allocator());
}

namespace detail {

template <typename Registry> struct runtime_lookup_identity_probe {
  template <typename Request, typename Interface, typename Cardinality,
            typename IdType = none_t>
  static bool selected_no_key_identity_matches(Registry &registry,
                                               IdType &&id = IdType()) {
    using key_domain = typename Registry::lookup_identity_key_domain;
    return selected_identity_matches<Request, Interface, Cardinality, void>(
        registry, key_domain::no_key, false, std::forward<IdType>(id));
  }

  template <typename Request, typename Interface, typename Key,
            typename Cardinality, typename IdType>
  static bool selected_typed_key_identity_matches(Registry &registry,
                                                  IdType &&id) {
    using key_domain = typename Registry::lookup_identity_key_domain;
    return selected_identity_matches<Request, Interface, Cardinality, Key>(
        registry, key_domain::typed_key, false, std::forward<IdType>(id));
  }

  template <typename Request, typename Interface, typename Key,
            typename Cardinality, typename IdType>
  static bool selected_runtime_key_identity_matches(Registry &registry,
                                                    IdType &&id) {
    using key_domain = typename Registry::lookup_identity_key_domain;
    return selected_identity_matches<Request, Interface, Cardinality, Key>(
        registry, key_domain::runtime_key, true, std::forward<IdType>(id));
  }

private:
  template <typename Request, typename Interface, typename Cardinality,
            typename Key, typename IdType>
  static bool selected_identity_matches(
      Registry &registry, typename Registry::lookup_identity_key_domain domain,
      bool expects_runtime_key_storage, IdType &&id) {
    auto selection = registry.template runtime_source_select<Request>(
        std::forward<IdType>(id));
    if (!selection.found() || !selection.state) {
      return false;
    }

    auto *entry = static_cast<typename Registry::registered_binding_entry *>(
        selection.state);
    if (Registry::lookup_identity_count(*entry) != 1) {
      return false;
    }

    const auto &identity = Registry::lookup_identity_at(*entry, 0);
    if (!(Registry::lookup_identity_interface_type(identity) ==
          Registry::rtti_type::template get_type_index<Interface>())) {
      return false;
    }
    if (Registry::lookup_identity_domain(identity) != domain) {
      return false;
    }
    if (Registry::lookup_identity_cardinality_value(identity) !=
        Registry::template lookup_cardinality<Cardinality>()) {
      return false;
    }

    const auto &key_type = Registry::lookup_identity_key_type(identity);
    if constexpr (std::is_void_v<Key>) {
      if (key_type) {
        return false;
      }
    } else {
      if (!key_type ||
          !(*key_type == Registry::rtti_type::template get_type_index<Key>())) {
        return false;
      }
    }

    return Registry::lookup_identity_has_runtime_key_storage(identity) ==
           expects_runtime_key_storage;
  }
};

} // namespace detail

} // namespace dingo
