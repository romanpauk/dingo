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
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/registration/requirements.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/lookup.h>
#include <dingo/runtime/lookup_index.h>
#include <dingo/static/local_resolution.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/request_traits.h>
#include <dingo/type/type_list.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace dingo {
namespace detail {

template <typename StaticRegistry, typename ParentContainer>
class container_with_static_bindings;

template <typename Registry> struct runtime_slot_key_probe;
template <typename Registry> struct runtime_slot_probe;

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
  template <typename RegistryT> friend struct detail::runtime_slot_key_probe;
  template <typename RegistryT> friend struct detail::runtime_slot_probe;

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
  using lookup_definition_type =
      detail::container_lookup_definition_type_t<ContainerTraits>;
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
              auto selection = select_runtime_binding<T>(state, data, id);
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
        select_runtime_binding<Request>(state, data, std::forward<IdType>(id));
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
  using normalized_lookup_entries =
      detail::normalize_lookup_definitions_t<lookup_definition_type>;
  using slot_key = detail::slot_key<rtti_type>;
  using slot_key_domain = detail::slot_domain;
  using slot_key_cardinality = detail::slot_cardinality;

  template <typename Cardinality>
  static constexpr slot_key_cardinality lookup_cardinality() {
    return detail::slot_cardinality_value<Cardinality>();
  }

  template <typename LookupEntry>
  using lookup_entry_cardinality =
      detail::lookup_entry_cardinality<LookupEntry>;

  struct registered_binding_entry : binding_cache_state {
    runtime_binding_ptr<runtime_binding_interface<container_type>> binding;
    typename rtti_type::type_index storage_type;
    std::optional<typename rtti_type::type_index> key_type;
    slot_key key;

    registered_binding_entry(
        runtime_binding_ptr<runtime_binding_interface<container_type>>
            &&binding_ptr,
        typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        slot_key &&resolved_slot_key)
        : binding(std::move(binding_ptr)),
          storage_type(std::move(resolved_storage_type)),
          key_type(std::move(resolved_key_type)),
          key(std::move(resolved_slot_key)) {}

    registered_binding_entry(const registered_binding_entry &) = delete;
    registered_binding_entry &
    operator=(const registered_binding_entry &) = delete;

    registered_binding_entry(registered_binding_entry &&other) noexcept
        : binding_cache_state{other.cache}, binding(std::move(other.binding)),
          storage_type(std::move(other.storage_type)),
          key_type(std::move(other.key_type)), key(std::move(other.key)) {
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
        key = std::move(other.key);
      }
      return *this;
    }
  };

  struct runtime_binding_entry_owner {
    using entry_allocator = typename std::allocator_traits<
        allocator_type>::template rebind_alloc<registered_binding_entry>;
    using entry_list = std::list<registered_binding_entry, entry_allocator>;

    struct entry_handle {
      registered_binding_entry *ptr = nullptr;
      typename entry_list::iterator list_iterator{};

      registered_binding_entry &operator*() const { return *ptr; }
    };

    explicit runtime_binding_entry_owner(allocator_type &allocator)
        : allocator_(std::addressof(allocator)),
          entries_(detail::make_lookup_storage_allocator<entry_allocator>(
              allocator)) {}

    runtime_binding_entry_owner(const runtime_binding_entry_owner &) = delete;
    runtime_binding_entry_owner &
    operator=(const runtime_binding_entry_owner &) = delete;

    bool empty() const { return entries_.empty(); }

    template <typename Binding>
    entry_handle emplace_entry(
        Binding &&binding, typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        slot_key &&resolved_slot_key) {
      entries_.emplace_back(
          std::forward<Binding>(binding), std::move(resolved_storage_type),
          std::move(resolved_key_type), std::move(resolved_slot_key));
      auto entry = std::prev(entries_.end());
      return {std::addressof(*entry), entry};
    }

    template <typename Binding, typename... Args>
    std::pair<entry_handle, typename Binding::container_type *>
    emplace_binding_entry(
        typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        slot_key &&resolved_slot_key, Args &&...args) {
      auto &&[binding, binding_container] = make_allocated_binding<Binding>(
          *allocator_, std::forward<Args>(args)...);
      try {
        return {emplace_entry(
                    std::move(binding), std::move(resolved_storage_type),
                    std::move(resolved_key_type), std::move(resolved_slot_key)),
                binding_container};
      } catch (...) {
        binding.reset();
        throw;
      }
    }

    void erase_entry(entry_handle entry) {
      entries_.erase(entry.list_iterator);
    }

  private:
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

    allocator_type *allocator_;
    entry_list entries_;
  };

  template <typename Interface, typename KeyDomain, typename Cardinality>
  struct runtime_binding_slot {};

  struct runtime_type_bindings {
    template <typename RegistryT> friend struct detail::runtime_slot_probe;

    using entry_handle = typename runtime_binding_entry_owner::entry_handle;

  private:
    template <typename LookupEntry> struct lookup_slot;

  public:
    template <typename LookupEntry> struct lookup_slot_tag {};

    template <typename LookupEntry>
    runtime_type_bindings(allocator_type &allocator,
                          runtime_binding_entry_owner &entry_owner,
                          lookup_slot_tag<LookupEntry> tag)
        : allocator_(std::addressof(allocator)),
          entry_owner_(std::addressof(entry_owner)) {
      (void)tag;
      construct_slot<lookup_slot<LookupEntry>>(allocator);
      validate_lookup_definitions();
    }

    runtime_type_bindings(const runtime_type_bindings &) = delete;
    runtime_type_bindings &operator=(const runtime_type_bindings &) = delete;

    ~runtime_type_bindings() { destroy_storage(); }

    template <typename Binding>
    entry_handle emplace_entry(
        Binding &&binding, typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        slot_key &&resolved_slot_key) {
      auto handle = entry_owner_->emplace_entry(
          std::forward<Binding>(binding), std::move(resolved_storage_type),
          std::move(resolved_key_type), std::move(resolved_slot_key));
      return handle;
    }

    template <typename Binding, typename... Args>
    std::pair<entry_handle, typename Binding::container_type *>
    emplace_binding_entry(
        typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        slot_key &&resolved_slot_key, Args &&...args) {
      auto result = entry_owner_->template emplace_binding_entry<Binding>(
          std::move(resolved_storage_type), std::move(resolved_key_type),
          std::move(resolved_slot_key), std::forward<Args>(args)...);
      return result;
    }

    void erase_entry(entry_handle entry) {
      if (entry_owner_) {
        entry_owner_->erase_entry(entry);
      }
    }

    template <typename LookupEntry, typename KeyArg>
    auto insert_lookup_rows(entry_handle entry, const KeyArg &key_arg) {
      return storage_as<lookup_slot<LookupEntry>>().insert(*entry, key_arg);
    }

    template <typename LookupEntry, typename InsertionHandle>
    void erase_lookup_rows(InsertionHandle handle) {
      storage_as<lookup_slot<LookupEntry>>().erase(handle);
    }

    template <typename LookupEntry, typename... KeyArgs>
    registered_binding_entry *find_singular(bool &ambiguous,
                                            KeyArgs &&...key_args) {
      if (is_storage<lookup_slot<LookupEntry>>()) {
        return storage_as<lookup_slot<LookupEntry>>().find_singular(
            std::forward<KeyArgs>(key_args)..., ambiguous);
      }
      ambiguous = false;
      return nullptr;
    }

    template <typename LookupEntry, typename Fn, typename... KeyArgs>
    std::size_t for_each(Fn &&fn, KeyArgs &&...key_args) {
      if (is_storage<lookup_slot<LookupEntry>>()) {
        return storage_as<lookup_slot<LookupEntry>>().for_each(
            std::forward<KeyArgs>(key_args)..., std::forward<Fn>(fn));
      }
      return 0;
    }

  private:
    static void validate_lookup_definitions() {
      using entries_type = normalized_lookup_entries;
      static_assert(!detail::has_duplicate_lookup_definition_v<entries_type>,
                    "duplicate dingo lookup definition for interface/key "
                    "domain/cardinality");
      static_assert(!detail::has_duplicate_lookup_slot_v<entries_type>,
                    "conflicting dingo lookup definitions for interface/key "
                    "domain");
    }

    template <typename LookupEntry>
    using slot_storage_type =
        detail::slot_storage<LookupEntry, registered_binding_entry,
                             allocator_type>;

    template <typename LookupEntry> struct lookup_slot {
      using storage_type = slot_storage_type<LookupEntry>;

      explicit lookup_slot(allocator_type &allocator) : storage(allocator) {}

      lookup_slot(const lookup_slot &) = delete;
      lookup_slot &operator=(const lookup_slot &) = delete;

      bool empty() const { return empty_storage(); }

      template <typename KeyArg>
      auto insert(registered_binding_entry &entry, const KeyArg &key_arg) {
        if (conflicts(entry, key_arg)) {
          return typename storage_type::insertion_result{false, {}};
        }
        return insert_row(entry, key_arg);
      }

      template <typename InsertionHandle> void erase(InsertionHandle handle) {
        erase_row(handle);
      }

      registered_binding_entry *find_singular(bool &ambiguous) {
        return storage.find_singular(ambiguous);
      }

      template <typename Fn> std::size_t for_each(Fn &&fn) {
        return storage.for_each(std::forward<Fn>(fn));
      }

      template <typename KeyValue>
      registered_binding_entry *find_singular(const KeyValue &key_value,
                                              bool &ambiguous) {
        return storage.find_singular(key_value, ambiguous);
      }

      template <typename KeyValue, typename Fn>
      std::size_t for_each(const KeyValue &key_value, Fn &&fn) {
        return storage.for_each(key_value, std::forward<Fn>(fn));
      }

      storage_type storage;

    private:
      bool empty_storage() const {
        if constexpr (detail::is_runtime_key_lookup_entry_v<LookupEntry>) {
          return true;
        } else {
          return storage.for_each([](auto &) {}) == 0;
        }
      }

      template <typename KeyArg>
      bool conflicts(const registered_binding_entry &entry,
                     const KeyArg &key_arg) const {
        return storage.conflicts(entry, key_arg);
      }

      template <typename KeyArg>
      auto insert_row(registered_binding_entry &entry, const KeyArg &key_arg) {
        return storage.insert(entry, key_arg);
      }

      template <typename InsertionHandle>
      void erase_row(InsertionHandle handle) {
        storage.erase(handle);
      }
    };

    template <typename Slot, typename... Args>
    void construct_slot(allocator_type &allocator, Args &&...args) {
      using slot_allocator = typename std::allocator_traits<
          allocator_type>::template rebind_alloc<Slot>;
      using slot_allocator_traits = std::allocator_traits<slot_allocator>;

      auto slot_alloc =
          detail::make_lookup_storage_allocator<slot_allocator>(allocator);
      auto *slot = slot_allocator_traits::allocate(slot_alloc, 1);
      try {
        slot_allocator_traits::construct(slot_alloc, slot, allocator,
                                         std::forward<Args>(args)...);
      } catch (...) {
        slot_allocator_traits::deallocate(slot_alloc, slot, 1);
        throw;
      }

      destroy_ = &destroy_allocated_slot<Slot>;
      storage_ = slot;
      storage_type_ = std::addressof(storage_type_token<Slot>());
    }

    void destroy_storage() noexcept {
      if (storage_) {
        destroy_(*allocator_, storage_);
        storage_ = nullptr;
      }
    }

    template <typename Slot>
    static void destroy_allocated_slot(allocator_type &allocator,
                                       void *storage) noexcept {
      using slot_allocator = typename std::allocator_traits<
          allocator_type>::template rebind_alloc<Slot>;
      using slot_allocator_traits = std::allocator_traits<slot_allocator>;

      auto slot_alloc =
          detail::make_lookup_storage_allocator<slot_allocator>(allocator);
      auto *slot = static_cast<Slot *>(storage);
      slot_allocator_traits::destroy(slot_alloc, slot);
      slot_allocator_traits::deallocate(slot_alloc, slot, 1);
    }

    template <typename Slot> static const int &storage_type_token() {
      static const int token = 0;
      return token;
    }

    template <typename Slot> bool is_storage() const {
      return storage_type_ == std::addressof(storage_type_token<Slot>());
    }

    template <typename Slot> Slot &storage_as() {
      return *static_cast<Slot *>(storage_);
    }

    template <typename Slot> const Slot &storage_as() const {
      return *static_cast<const Slot *>(storage_);
    }

    allocator_type *allocator_ = nullptr;
    void *storage_ = nullptr;
    runtime_binding_entry_owner *entry_owner_ = nullptr;
    void (*destroy_)(allocator_type &, void *) noexcept = nullptr;
    const void *storage_type_ = nullptr;
  };

  struct runtime_bindings_state {
    explicit runtime_bindings_state(allocator_type &allocator)
        : entry_owner(allocator), type_bindings(allocator),
          type_cache(allocator), lookup_indexes(allocator) {}

    runtime_binding_entry_owner entry_owner;
    typename ContainerTraits::template type_map_type<runtime_type_bindings,
                                                     allocator_type>
        type_bindings;
    typename ContainerTraits::template type_cache_type<void *, allocator_type>
        type_cache;
    detail::lookup_index<normalized_lookup_entries, registered_binding_entry *,
                         allocator_type>
        lookup_indexes;
  };

  struct runtime_bindings_state_storage {
    runtime_bindings_state *get() { return state_; }
    const runtime_bindings_state *get() const { return state_; }

    runtime_bindings_state &ensure(allocator_type &allocator) {
      if (!state_) {
        auto alloc =
            allocator_traits::rebind<runtime_bindings_state>(allocator);
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
        auto alloc =
            allocator_traits::rebind<runtime_bindings_state>(allocator);
        allocator_traits::destroy(alloc, state_);
        allocator_traits::deallocate(alloc, state_, 1);
        state_ = nullptr;
      }
    }

    runtime_bindings_state *state_ = nullptr;
  };

  using runtime_binding_interface_type =
      runtime_binding_interface<container_type>;
  using runtime_selection =
      detail::runtime_binding_selection<runtime_binding_interface_type,
                                        binding_cache_state *>;

  static std::size_t slot_key_count(const registered_binding_entry &entry) {
    (void)entry;
    return 1;
  }

  static const slot_key &slot_key_at(const registered_binding_entry &entry,
                                     std::size_t index) {
    (void)index;
    return entry.key;
  }

  static const typename rtti_type::type_index &
  slot_key_interface_type(const slot_key &key) {
    return key.interface_type;
  }

  static slot_key_domain slot_key_domain_value(const slot_key &key) {
    return key.domain;
  }

  static slot_key_cardinality slot_key_cardinality_value(const slot_key &key) {
    return key.cardinality;
  }

  static const typename rtti_type::type_index &
  slot_key_key_type(const slot_key &key) {
    return key.key_type;
  }

  static typename rtti_type::type_index no_slot_key_type() {
    return detail::no_slot_key_type<rtti_type>();
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
                                             normalized_lookup_entries>;

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
  using no_key_lookup_slot_entry_t = std::conditional_t<
      std::is_void_v<no_key_lookup_entry_t<Interface, Cardinality>>,
      detail::lookup_entry<normalized_type_t<Interface>, ::dingo::no_key,
                           Cardinality, detail::no_lookup_backend>,
      no_key_lookup_entry_t<Interface, Cardinality>>;

  template <typename Interface, typename Key, typename Cardinality>
  using typed_key_lookup_entry_t =
      detail::selected_typed_key_lookup_entry_t<Interface, Key, Cardinality,
                                                normalized_lookup_entries>;

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
  using typed_key_lookup_slot_entry_t = std::conditional_t<
      std::is_void_v<typed_key_lookup_entry_t<Interface, Key, Cardinality>>,
      detail::lookup_entry<
          normalized_type_t<Interface>,
          ::dingo::typed_key<detail::normalized_lookup_key_t<Key>>, Cardinality,
          detail::no_lookup_backend>,
      typed_key_lookup_entry_t<Interface, Key, Cardinality>>;

  template <typename Interface, typename KeyDomain, typename Cardinality>
  struct selected_lookup_slot_entry;

  template <typename Interface, typename Cardinality>
  struct selected_lookup_slot_entry<Interface, ::dingo::no_key, Cardinality> {
    using type = no_key_lookup_slot_entry_t<Interface, Cardinality>;
  };

  template <typename Interface, typename Key, typename Cardinality>
  struct selected_lookup_slot_entry<Interface, ::dingo::typed_key<Key>,
                                    Cardinality> {
    using type = typed_key_lookup_slot_entry_t<Interface, Key, Cardinality>;
  };

  template <typename Interface, typename KeyDomain, typename Cardinality>
  using selected_lookup_slot_entry_t =
      typename selected_lookup_slot_entry<Interface, KeyDomain,
                                          Cardinality>::type;

  template <typename Interface, typename KeyDomain, typename Cardinality>
  struct lookup_runtime_binding_slot;

  template <typename Interface, typename Cardinality>
  struct lookup_runtime_binding_slot<Interface, ::dingo::no_key, Cardinality> {
    using type = runtime_binding_slot<Interface, ::dingo::no_key, Cardinality>;
  };

  template <typename Interface, typename Key, typename Cardinality>
  struct lookup_runtime_binding_slot<Interface, ::dingo::typed_key<Key>,
                                     Cardinality> {
    using type = runtime_binding_slot<
        Interface, ::dingo::typed_key<detail::normalized_lookup_key_t<Key>>,
        Cardinality>;
  };

  template <typename Interface, typename Key, typename Cardinality>
  struct lookup_runtime_binding_slot<Interface, ::dingo::runtime_key<Key>,
                                     Cardinality> {
    using type = runtime_binding_slot<
        Interface, ::dingo::runtime_key<detail::normalized_lookup_key_t<Key>>,
        Cardinality>;
  };

  template <typename Interface, typename KeyDomain, typename Cardinality>
  using lookup_runtime_binding_slot_t =
      typename lookup_runtime_binding_slot<Interface, KeyDomain,
                                           Cardinality>::type;

  template <typename Interface, typename KeyDomain, typename Cardinality>
  runtime_type_bindings *
  lookup_slot_bindings_at(runtime_bindings_state *state) {
    if (!state) {
      return nullptr;
    }
    using slot =
        lookup_runtime_binding_slot_t<Interface, KeyDomain, Cardinality>;
    return state->type_bindings.template get<slot>();
  }

  template <typename Interface, typename Key>
  using runtime_key_lookup_entry_t =
      detail::selected_lookup_entry_t<Interface, Key,
                                      normalized_lookup_entries>;

  template <typename Interface, typename Key>
  using runtime_key_lookup_cardinality_t = typename lookup_entry_cardinality<
      runtime_key_lookup_entry_t<Interface, Key>>::type;

  template <typename Interface, typename Key, typename LookupEntry,
            bool Valid = !std::is_void_v<LookupEntry>>
  struct runtime_request_slot_type {
    using type = void;
  };

  template <typename Interface, typename Key, typename LookupEntry>
  struct runtime_request_slot_type<Interface, Key, LookupEntry, true> {
    using type = lookup_runtime_binding_slot_t<
        Interface, ::dingo::runtime_key<Key>,
        typename lookup_entry_cardinality<LookupEntry>::type>;
  };

  template <typename Interface, typename IdType> struct request_slot_traits;

  template <typename Interface>
  struct request_slot_traits<Interface, ::dingo::none_t> {
    using normalized_interface = normalized_type_t<Interface>;
    using key_domain = ::dingo::no_key;
    using runtime_key_type = void;
    using selected_cardinality =
        no_key_lookup_cardinality_t<normalized_interface>;
    using lookup_entry =
        no_key_lookup_slot_entry_t<normalized_interface, selected_cardinality>;
    using slot_type =
        lookup_runtime_binding_slot_t<normalized_interface, key_domain,
                                      selected_cardinality>;
    static constexpr bool has_valid_explicit_runtime_key_view = false;
    static constexpr bool has_explicit_collection_lookup =
        has_explicit_no_key_many_lookup_v<normalized_interface>;
  };

  template <typename Interface, typename Key>
  struct request_slot_traits<Interface, ::dingo::key<Key>> {
    using normalized_interface = normalized_type_t<Interface>;
    using key_type = Key;
    using key_domain = ::dingo::typed_key<key_type>;
    using runtime_key_type = void;
    using selected_cardinality =
        typed_key_lookup_cardinality_t<normalized_interface, key_type>;
    using lookup_entry =
        typed_key_lookup_slot_entry_t<normalized_interface, key_type,
                                      selected_cardinality>;
    using slot_type =
        lookup_runtime_binding_slot_t<normalized_interface, key_domain,
                                      selected_cardinality>;
    static constexpr bool has_valid_explicit_runtime_key_view = false;
    static constexpr bool has_explicit_collection_lookup =
        std::is_same_v<selected_cardinality, ::dingo::many>;
  };

  template <typename Interface, typename IdType> struct request_slot_traits {
    using normalized_interface = normalized_type_t<Interface>;
    using runtime_key_type = std::decay_t<IdType>;
    using key_domain = ::dingo::runtime_key<runtime_key_type>;
    using lookup_entry =
        runtime_key_lookup_entry_t<normalized_interface, runtime_key_type>;
    using selected_cardinality =
        typename lookup_entry_cardinality<lookup_entry>::type;
    using slot_type = typename runtime_request_slot_type<
        normalized_interface, runtime_key_type, lookup_entry>::type;
    static constexpr bool has_valid_explicit_runtime_key_view =
        !std::is_void_v<lookup_entry>;
    static constexpr bool has_explicit_collection_lookup =
        has_valid_explicit_runtime_key_view &&
        std::is_same_v<selected_cardinality, ::dingo::many>;
  };

  template <typename T, typename IdType>
  static constexpr bool has_explicit_collection_lookup(IdType &&) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using traits = request_slot_traits<resolve_type, std::decay_t<IdType>>;
    return traits::has_explicit_collection_lookup;
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

    using traits = request_slot_traits<Interface, std::decay_t<IdType>>;
    if constexpr (std::is_void_v<typename traits::slot_type>) {
      (void)state;
      return nullptr;
    } else {
      return state->type_bindings.template get<typename traits::slot_type>();
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
    return select_runtime_binding<Request>(state, data,
                                           std::forward<IdType>(id));
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
  runtime_type_bindings *
  runtime_collection_bindings(runtime_bindings_state *state, IdType &&) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    return runtime_source_bindings<resolve_type, IdType>(state);
  }

  template <typename T, typename IdType>
  runtime_type_bindings *runtime_collection_bindings(IdType &&id) {
    return runtime_collection_bindings<T>(runtime_bindings_if_present(),
                                          std::forward<IdType>(id));
  }

  template <typename T, typename IdType, typename Fn>
  std::size_t for_each_collection_entry(runtime_bindings_state *state,
                                        runtime_type_bindings &data,
                                        IdType &&id, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;
    using traits = request_slot_traits<resolve_type, std::decay_t<IdType>>;

    if constexpr (traits::has_explicit_collection_lookup) {
      if constexpr (!is_none_v<std::decay_t<IdType>> &&
                    !detail::is_typed_key_v<IdType>) {
        (void)data;
        if (!state) {
          return 0;
        }
        return for_each_lookup_index_entry<typename traits::lookup_entry>(
            *state, std::forward<IdType>(id), std::forward<Fn>(fn));
      } else {
        (void)id;
        return data.template for_each<typename traits::lookup_entry>(
            std::forward<Fn>(fn));
      }
    } else if constexpr (is_none_v<std::decay_t<IdType>>) {
      if constexpr (!has_explicit_no_key_one_lookup_v<index_interface_type>) {
        return data.template for_each<
            no_key_lookup_slot_entry_t<index_interface_type, ::dingo::one>>(
            std::forward<Fn>(fn));
      } else {
        (void)data;
        (void)id;
        (void)fn;
        return 0;
      }
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      using key_type = typename std::decay_t<IdType>::type;
      if constexpr (!has_explicit_typed_key_one_lookup_v<index_interface_type,
                                                         key_type>) {
        (void)id;
        return data.template for_each<typed_key_lookup_slot_entry_t<
            index_interface_type, key_type, ::dingo::one>>(
            std::forward<Fn>(fn));
      } else {
        (void)data;
        (void)id;
        (void)fn;
        return 0;
      }
    } else {
      (void)data;
      (void)id;
      (void)fn;
      return 0;
    }
  }

  template <typename T, typename Fn, typename IdType>
  std::size_t append_runtime_collection(T &results, runtime_context &context,
                                        Fn &&fn, IdType id) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    auto *state = runtime_bindings_if_present();
    auto *data = runtime_collection_bindings<T>(state, id);
    if (!data) {
      return 0;
    }
    return for_each_collection_entry<T>(state, *data, id, [&](auto &entry) {
      fn(results,
         resolve_collection_type<resolve_type>(*entry.binding, context));
    });
  }

  template <typename T, typename IdType>
  std::size_t count_runtime_collection(IdType id) {
    auto *state = runtime_bindings_if_present();
    auto *data = runtime_collection_bindings<T>(state, id);
    if (!data) {
      return 0;
    }
    std::size_t result = 0;
    for_each_collection_entry<T>(state, *data, id, [&](auto &) { ++result; });
    return result;
  }

private:
  template <typename Request, typename IdType>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  runtime_selection select_runtime_binding(runtime_bindings_state *state,
                                           runtime_type_bindings *data,
                                           IdType &&id) {
    using traits = request_slot_traits<Request, std::decay_t<IdType>>;
    using lookup_entry = typename traits::lookup_entry;
    if constexpr (std::is_void_v<lookup_entry>) {
      if constexpr (!std::is_same_v<void, ParentRegistry> &&
                    !std::is_base_of_v<registry_type, resolve_root_type>) {
        return detail::make_runtime_selection<runtime_binding_interface_type,
                                              binding_cache_state *>(nullptr,
                                                                     nullptr);
      } else {
        static_assert(!std::is_void_v<lookup_entry>,
                      "keyed registration or request has no matching "
                      "dingo lookup definition for interface/key");
      }
    } else {
      if constexpr (!is_none_v<std::decay_t<IdType>> &&
                    !detail::is_typed_key_v<IdType>) {
        (void)data;
        return select_lookup_index_binding<lookup_entry>(
            state, std::forward<IdType>(id));
      } else {
        (void)state;
        (void)id;
        return select_slot_binding<lookup_entry>(data);
      }
    }
  }

  template <typename LookupEntry, typename... KeyArgs>
  runtime_selection select_slot_binding(runtime_type_bindings *data,
                                        KeyArgs &&...key_args) {
    registered_binding_entry *selected = nullptr;
    bool ambiguous = false;
    if (data) {
      selected = data->template find_singular<LookupEntry>(
          ambiguous, std::forward<KeyArgs>(key_args)...);
    }

    return make_slot_selection(selected, ambiguous);
  }

  runtime_selection make_slot_selection(registered_binding_entry *selected,
                                        bool ambiguous) {
    if (ambiguous) {
      return runtime_selection::ambiguity();
    }
    return detail::make_runtime_selection<runtime_binding_interface_type,
                                          binding_cache_state *>(
        selected ? selected->binding.get() : nullptr, selected);
  }

  template <typename Value>
  static auto lookup_index_mapped(Value &value, int)
      -> decltype((value.second)) {
    return value.second;
  }

  template <typename Value>
  static Value &lookup_index_mapped(Value &value, ...) {
    return value;
  }

  template <typename Iterator>
  static decltype(auto) lookup_index_iterator_mapped(Iterator iterator) {
    return lookup_index_mapped(*iterator, 0);
  }

  template <typename LookupEntry, typename Backend, typename Key>
  static registered_binding_entry *lookup_index_find_singular(Backend &backend,
                                                              const Key &key,
                                                              bool &ambiguous) {
    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto it = backend.find(key);
      ambiguous = false;
      return it == backend.end() ? nullptr : lookup_index_iterator_mapped(it);
    } else {
      auto [first, last] = backend.equal_range(key);
      if (first == last) {
        ambiguous = false;
        return nullptr;
      }
      auto next = first;
      ++next;
      ambiguous = next != last;
      return ambiguous ? nullptr : lookup_index_iterator_mapped(first);
    }
  }

  template <typename LookupEntry, typename Backend, typename Key, typename Fn>
  static std::size_t lookup_index_for_each(Backend &backend, const Key &key,
                                           Fn &&fn) {
    if constexpr (std::is_same_v<
                      typename lookup_entry_cardinality<LookupEntry>::type,
                      ::dingo::one>) {
      auto it = backend.find(key);
      if (it == backend.end()) {
        return 0;
      }
      fn(*lookup_index_iterator_mapped(it));
      return 1;
    } else {
      std::size_t result = 0;
      auto [first, last] = backend.equal_range(key);
      for (auto it = first; it != last; ++it) {
        fn(*lookup_index_iterator_mapped(it));
        ++result;
      }
      return result;
    }
  }

  template <typename LookupEntry, typename Key>
  runtime_selection select_lookup_index_binding(runtime_bindings_state *state,
                                                const Key &key) {
    registered_binding_entry *selected = nullptr;
    bool ambiguous = false;
    if (state) {
      auto &index = state->lookup_indexes.template get<LookupEntry>();
      selected = lookup_index_find_singular<LookupEntry>(index, key, ambiguous);
    }
    return make_slot_selection(selected, ambiguous);
  }

  template <typename LookupEntry, typename Key, typename Fn>
  std::size_t for_each_lookup_index_entry(runtime_bindings_state &state,
                                          const Key &key, Fn &&fn) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    return lookup_index_for_each<LookupEntry>(index, key, std::forward<Fn>(fn));
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

        if constexpr (!is_none_v<std::decay_t<Arg>>) {
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

  template <typename Slot, typename Cardinality, typename LookupEntry>
  runtime_type_bindings &ensure_lookup_slot_bindings() {
    static_assert(
        std::is_same_v<typename lookup_entry_cardinality<LookupEntry>::type,
                       Cardinality>,
        "lookup slot LookupEntry cardinality must match slot cardinality");
    auto &state = ensure_runtime_bindings();
    return state.type_bindings
        .template insert<Slot>(
            get_allocator(), state.entry_owner,
            typename runtime_type_bindings::template lookup_slot_tag<
                LookupEntry>{})
        .first;
  }

  template <typename Interface, typename KeyDomain, typename Cardinality>
  runtime_type_bindings &ensure_lookup_slot_at() {
    using lookup_entry =
        selected_lookup_slot_entry_t<Interface, KeyDomain, Cardinality>;
    using slot =
        lookup_runtime_binding_slot_t<Interface, KeyDomain, Cardinality>;
    return ensure_lookup_slot_bindings<slot, Cardinality, lookup_entry>();
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupEntry,
            typename KeyArg, typename ExceptionFactory>
  void commit_registration_entry(
      runtime_type_bindings &data,
      typename runtime_type_bindings::entry_handle inserted_entry,
      const KeyArg &key_arg, ExceptionFactory &&make_exception) {
    bool binding_registered = true;
    bool lookup_rows_registered = false;
    using insertion_result_type =
        decltype(data.template insert_lookup_rows<LookupEntry>(inserted_entry,
                                                               key_arg));
    using insertion_handle_type =
        decltype(std::declval<insertion_result_type>().handle);
    std::optional<insertion_handle_type> lookup_rows_handle;
    try {
      auto lookup_rows_insert = data.template insert_lookup_rows<LookupEntry>(
          inserted_entry, key_arg);
      if (!lookup_rows_insert) {
        data.erase_entry(inserted_entry);
        binding_registered = false;
        throw make_exception();
      }
      lookup_rows_handle = lookup_rows_insert.handle;
      lookup_rows_registered = true;
    } catch (...) {
      if (binding_registered) {
        if (lookup_rows_registered) {
          data.template erase_lookup_rows<LookupEntry>(*lookup_rows_handle);
        }
        data.erase_entry(inserted_entry);
      }
      throw;
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupEntry,
            typename KeyArg, typename ExceptionFactory>
  void commit_runtime_key_registration_entry(
      runtime_bindings_state &state, runtime_type_bindings &data,
      typename runtime_type_bindings::entry_handle inserted_entry,
      const KeyArg &key_arg, ExceptionFactory &&make_exception) {
    auto &index = state.lookup_indexes.template get<LookupEntry>();
    using index_type = std::remove_reference_t<decltype(index)>;
    using insertion_handle_type = typename index_type::iterator;
    std::optional<insertion_handle_type> lookup_row_handle;
    bool binding_registered = true;
    bool lookup_row_registered = false;

    try {
      if constexpr (std::is_same_v<
                        typename lookup_entry_cardinality<LookupEntry>::type,
                        ::dingo::one>) {
        auto [it, inserted] =
            index.try_emplace(key_arg, std::addressof(*inserted_entry));
        if (!inserted) {
          data.erase_entry(inserted_entry);
          binding_registered = false;
          throw make_exception();
        }
        lookup_row_handle = it;
      } else {
        lookup_row_handle =
            index.emplace(key_arg, std::addressof(*inserted_entry));
      }
      lookup_row_registered = true;
    } catch (...) {
      if (binding_registered) {
        if (lookup_row_registered) {
          index.erase(*lookup_row_handle);
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

  template <typename TypeInterface, typename TypeStorage, typename LookupEntry,
            typename KeyArg, typename EntryFactory, typename ExceptionFactory>
  auto register_slot_binding_transaction(
      runtime_type_bindings &data,
      typename rtti_type::type_index resolved_storage_type,
      std::optional<typename rtti_type::type_index> resolved_key_type,
      slot_key &&lookup_id, const KeyArg &key_arg, EntryFactory &&entry_factory,
      ExceptionFactory &&make_exception) {
    auto insertion =
        entry_factory(data, std::move(resolved_storage_type),
                      std::move(resolved_key_type), std::move(lookup_id));
    commit_registration_entry<TypeInterface, TypeStorage, LookupEntry>(
        data, insertion.inserted_entry, key_arg,
        std::forward<ExceptionFactory>(make_exception));
    runtime_bindings_present_ = true;
    return insertion;
  }

  template <typename TypeInterface, typename TypeStorage, typename LookupEntry,
            typename KeyArg, typename EntryFactory, typename ExceptionFactory>
  auto register_runtime_key_binding_transaction(
      runtime_bindings_state &state, runtime_type_bindings &data,
      typename rtti_type::type_index resolved_storage_type,
      std::optional<typename rtti_type::type_index> resolved_key_type,
      slot_key &&lookup_id, const KeyArg &key_arg, EntryFactory &&entry_factory,
      ExceptionFactory &&make_exception) {
    auto insertion =
        entry_factory(data, std::move(resolved_storage_type),
                      std::move(resolved_key_type), std::move(lookup_id));
    commit_runtime_key_registration_entry<TypeInterface, TypeStorage,
                                          LookupEntry>(
        state, data, insertion.inserted_entry, key_arg,
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
  auto register_lookup_slot_binding(
      typename rtti_type::type_index resolved_storage_type,
      std::optional<typename rtti_type::type_index> resolved_key_type,
      slot_key &&lookup_id, EntryFactory &&entry_factory) {
    auto &data = ensure_lookup_slot_at<TypeInterface, KeyDomain, Cardinality>();
    using lookup_entry =
        selected_lookup_slot_entry_t<TypeInterface, KeyDomain, Cardinality>;
    return register_slot_binding_transaction<TypeInterface, TypeStorage,
                                             lookup_entry>(
        data, std::move(resolved_storage_type), std::move(resolved_key_type),
        std::move(lookup_id), none_t{},
        std::forward<EntryFactory>(entry_factory), [] {
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
                    "dingo lookup definition for interface/key");
      using lookup_cardinality_type =
          runtime_key_lookup_cardinality_t<TypeInterface, IdType>;
      using slot = lookup_runtime_binding_slot_t<
          TypeInterface, ::dingo::runtime_key<IdType>, lookup_cardinality_type>;
      auto &state = ensure_runtime_bindings();
      auto &data =
          state.type_bindings
              .template insert<slot>(
                  get_allocator(), state.entry_owner,
                  typename runtime_type_bindings::template lookup_slot_tag<
                      lookup_entry>{})
              .first;
      auto resolved_key_type = [&]() {
        if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
          return std::optional<typename rtti_type::type_index>{};
        } else {
          return std::optional<typename rtti_type::type_index>{
              rtti_type::template get_type_index<std::decay_t<KeyIdType>>()};
        }
      }();
      auto lookup_id = detail::make_slot_key<rtti_type, TypeInterface,
                                             ::dingo::runtime_key<IdType>,
                                             lookup_cardinality_type>();
      return register_runtime_key_binding_transaction<
          TypeInterface, TypeStorage, lookup_entry>(
          state, data, std::move(resolved_storage_type),
          std::move(resolved_key_type), std::move(lookup_id), id,
          std::forward<EntryFactory>(entry_factory), [] {
            return make_registration_duplicate_exception<
                TypeInterface, TypeStorage, IdType, KeyIdType>();
          });
    } else if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
      using lookup_cardinality_type =
          no_key_lookup_cardinality_t<TypeInterface>;
      auto lookup_id =
          detail::make_slot_key<rtti_type, TypeInterface, ::dingo::no_key,
                                lookup_cardinality_type>();
      return register_lookup_slot_binding<
          TypeInterface, TypeStorage, ::dingo::no_key, lookup_cardinality_type,
          IdType, KeyIdType>(std::move(resolved_storage_type),
                             std::optional<typename rtti_type::type_index>{},
                             std::move(lookup_id),
                             std::forward<EntryFactory>(entry_factory));
    } else {
      using typed_key_type = typename std::decay_t<KeyIdType>::type;
      using lookup_cardinality_type =
          typed_key_lookup_cardinality_t<TypeInterface, typed_key_type>;
      auto lookup_id = detail::make_slot_key<rtti_type, TypeInterface,
                                             ::dingo::typed_key<typed_key_type>,
                                             lookup_cardinality_type>();
      return register_lookup_slot_binding<
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
              data.template emplace_binding_entry<Binding>(
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
    try {
      allocator_traits::construct(alloc, instance, std::forward<Args>(args)...);
    } catch (...) {
      allocator_traits::deallocate(alloc, instance, 1);
      throw;
    }

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

template <typename Registry> struct runtime_slot_key_probe {
  template <typename Request, typename Interface, typename Cardinality,
            typename IdType = none_t>
  static bool selected_no_key_slot_key_matches(Registry &registry,
                                               IdType &&id = IdType()) {
    using key_domain = typename Registry::slot_key_domain;
    return selected_slot_key_matches<Request, Interface, Cardinality, void>(
        registry, key_domain::no_key, std::forward<IdType>(id));
  }

  template <typename Request, typename Interface, typename Key,
            typename Cardinality, typename IdType>
  static bool selected_typed_key_slot_key_matches(Registry &registry,
                                                  IdType &&id) {
    using key_domain = typename Registry::slot_key_domain;
    return selected_slot_key_matches<Request, Interface, Cardinality, Key>(
        registry, key_domain::typed_key, std::forward<IdType>(id));
  }

  template <typename Request, typename Interface, typename Key,
            typename Cardinality, typename IdType>
  static bool selected_runtime_key_slot_key_matches(Registry &registry,
                                                    IdType &&id) {
    using key_domain = typename Registry::slot_key_domain;
    return selected_slot_key_matches<Request, Interface, Cardinality, Key>(
        registry, key_domain::runtime_key, std::forward<IdType>(id));
  }

private:
  template <typename Request, typename Interface, typename Cardinality,
            typename Key, typename IdType>
  static bool
  selected_slot_key_matches(Registry &registry,
                            typename Registry::slot_key_domain domain,
                            IdType &&id) {
    auto selection = registry.template runtime_source_select<Request>(
        std::forward<IdType>(id));
    if (!selection.found() || !selection.state) {
      return false;
    }

    auto *entry = static_cast<typename Registry::registered_binding_entry *>(
        selection.state);
    if (Registry::slot_key_count(*entry) != 1) {
      return false;
    }

    const auto &key = Registry::slot_key_at(*entry, 0);
    if (!(Registry::slot_key_interface_type(key) ==
          Registry::rtti_type::template get_type_index<Interface>())) {
      return false;
    }
    if (Registry::slot_key_domain_value(key) != domain) {
      return false;
    }
    if (Registry::slot_key_cardinality_value(key) !=
        Registry::template lookup_cardinality<Cardinality>()) {
      return false;
    }

    const auto &key_type = Registry::slot_key_key_type(key);
    if constexpr (std::is_void_v<Key>) {
      if (!(key_type == Registry::no_slot_key_type())) {
        return false;
      }
    } else {
      if (!(key_type == Registry::rtti_type::template get_type_index<Key>())) {
        return false;
      }
    }

    return true;
  }
};

template <typename Registry> struct runtime_slot_probe {
  using entry_owner_type =
      decltype(std::declval<typename Registry::runtime_bindings_state &>()
                   .entry_owner);
  using expected_entry_owner_type =
      typename Registry::runtime_binding_entry_owner;
  using lookup_index_type =
      decltype(std::declval<typename Registry::runtime_bindings_state &>()
                   .lookup_indexes);
  using expected_lookup_index_type =
      lookup_index<typename Registry::normalized_lookup_entries,
                   typename Registry::registered_binding_entry *,
                   typename Registry::allocator_type>;

  static constexpr bool runtime_bindings_state_has_entry_owner() {
    return std::is_same_v<entry_owner_type, expected_entry_owner_type>;
  }

  static constexpr bool runtime_bindings_state_has_lookup_index() {
    return std::is_same_v<lookup_index_type, expected_lookup_index_type>;
  }

  template <typename LookupEntry>
  using lookup_slot_type =
      typename Registry::runtime_type_bindings::template lookup_slot<
          LookupEntry>;

  template <typename LookupEntry>
  using slot_storage_type = std::remove_reference_t<
      decltype(std::declval<lookup_slot_type<LookupEntry> &>().storage)>;

  template <typename LookupEntry>
  using expected_slot_storage_type =
      typename Registry::runtime_type_bindings::template slot_storage_type<
          LookupEntry>;
  using entry_owner_pointer_type =
      decltype(std::declval<typename Registry::runtime_type_bindings &>()
                   .entry_owner_);

  template <typename LookupEntry>
  static constexpr bool lookup_slot_is_concrete() {
    return std::is_same_v<slot_storage_type<LookupEntry>,
                          expected_slot_storage_type<LookupEntry>>;
  }

  static constexpr bool runtime_type_bindings_uses_shared_entry_owner() {
    return std::is_same_v<entry_owner_pointer_type,
                          expected_entry_owner_type *>;
  }

  template <typename Interface, typename Cardinality>
  static constexpr bool no_key_slot_is_lookup_slot() {
    using lookup_entry =
        typename Registry::template no_key_lookup_slot_entry_t<Interface,
                                                               Cardinality>;
    return lookup_slot_is_concrete<lookup_entry>();
  }

  template <typename Interface, typename Key, typename Cardinality>
  static constexpr bool typed_key_slot_is_lookup_slot() {
    using lookup_entry =
        typename Registry::template typed_key_lookup_slot_entry_t<
            Interface, Key, Cardinality>;
    return lookup_slot_is_concrete<lookup_entry>();
  }

  template <typename Interface, typename Key>
  static constexpr bool runtime_key_slot_is_lookup_slot() {
    using lookup_entry =
        typename Registry::template runtime_key_lookup_entry_t<Interface, Key>;
    return lookup_slot_is_concrete<lookup_entry>();
  }

  template <typename Interface, typename Key>
  using runtime_key_lookup_entry =
      typename Registry::template runtime_key_lookup_entry_t<Interface, Key>;

  template <typename LookupEntry>
  static bool lookup_slot_empty(Registry &registry) {
    auto *state = registry.runtime_bindings_if_present();
    if (!state) {
      return true;
    }

    using slot = typename Registry::template lookup_runtime_binding_slot_t<
        typename LookupEntry::interface_type, typename LookupEntry::key_domain,
        typename LookupEntry::cardinality>;
    auto *data = state->type_bindings.template get<slot>();
    if (!data) {
      return true;
    }

    if constexpr (detail::is_runtime_key_lookup_entry_v<LookupEntry>) {
      return state->entry_owner.empty();
    }

    return data->template storage_as<lookup_slot_type<LookupEntry>>().empty();
  }

  template <typename LookupEntry, typename Key>
  static std::size_t lookup_index_row_count(Registry &registry,
                                            const Key &key) {
    auto *state = registry.runtime_bindings_if_present();
    if (!state) {
      return 0;
    }

    auto &index = state->lookup_indexes.template get<LookupEntry>();
    if constexpr (std::is_same_v<
                      typename Registry::template lookup_entry_cardinality<
                          LookupEntry>::type,
                      ::dingo::one>) {
      return index.find(key) == index.end() ? 0 : 1;
    } else {
      std::size_t result = 0;
      auto [first, last] = index.equal_range(key);
      for (auto it = first; it != last; ++it) {
        ++result;
      }
      return result;
    }
  }
};

} // namespace detail

} // namespace dingo
