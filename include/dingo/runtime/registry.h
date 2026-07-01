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
            using lookup_type = normalized_type_t<T>;
            using exact_type = std::remove_cv_t<
                std::remove_reference_t<request_interface_t<T>>>;
            auto data = [&]() {
              if constexpr (detail::is_typed_key_v<IdType>) {
                return state->type_bindings.template get<exact_type>();
              } else {
                return state->type_bindings.template get<lookup_type>();
              }
            }();
            if constexpr (!std::is_same_v<lookup_type, exact_type>) {
              if (!data) {
                if constexpr (detail::is_typed_key_v<IdType>) {
                  data = state->type_bindings.template get<lookup_type>();
                } else {
                  data = state->type_bindings.template get<exact_type>();
                }
              }
            }
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
    using lookup_type = normalized_type_t<Request>;
    using exact_type =
        std::remove_cv_t<std::remove_reference_t<request_interface_t<Request>>>;
    auto *state = runtime_bindings_if_present();
    auto *data = [&]() {
      if (!state) {
        return static_cast<runtime_type_bindings *>(nullptr);
      }
      if constexpr (!is_none_v<std::decay_t<IdType>> &&
                    !detail::is_typed_key_v<IdType>) {
        return state->type_bindings.template get<lookup_type>();
      } else {
        return state->type_bindings.template get<exact_type>();
      }
    }();
    if constexpr (!std::is_same_v<lookup_type, exact_type>) {
      if (!data && state) {
        if constexpr (!is_none_v<std::decay_t<IdType>> &&
                      !detail::is_typed_key_v<IdType>) {
          data = state->type_bindings.template get<exact_type>();
        } else {
          data = state->type_bindings.template get<lookup_type>();
        }
      }
    }
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

    struct entry_handle {
      registered_binding_entry *ptr = nullptr;
      typename entry_list::iterator list_iterator{};
      bool inline_entry = false;

      registered_binding_entry &operator*() const { return *ptr; }
    };

    runtime_type_bindings(allocator_type &allocator)
        : allocator_(std::addressof(allocator)),
          entries(detail::make_lookup_storage_allocator<entry_allocator>(
              allocator)),
          lookup_table(allocator) {
      using entries_type = normalized_view_entries;
      static_assert(!detail::has_duplicate_lookup_definition_v<entries_type>,
                    "duplicate dingo view definition for interface/key "
                    "domain/cardinality");
      static_assert(!detail::has_duplicate_lookup_slot_v<entries_type>,
                    "conflicting dingo view definitions for interface/key "
                    "domain");
    }

    runtime_type_bindings(const runtime_type_bindings &) = delete;
    runtime_type_bindings &operator=(const runtime_type_bindings &) = delete;

    ~runtime_type_bindings() {
      erase_no_key_singular_entry();
      erase_inline_entry();
    }

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
            std::move(binding), std::move(resolved_storage_type), std::nullopt,
            std::move(resolved_lookup_identity));
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

    template <typename Binding>
    entry_handle emplace_entry(
        Binding &&binding, typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        lookup_identity &&resolved_lookup_identity) {
      if (!inline_entry_constructed_) {
        new (inline_entry_storage_) registered_binding_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::move(resolved_key_type), std::move(resolved_lookup_identity));
        inline_entry_constructed_ = true;
        return {inline_entry(), {}, true};
      }

      entries.emplace_back(
          std::forward<Binding>(binding), std::move(resolved_storage_type),
          std::move(resolved_key_type), std::move(resolved_lookup_identity));
      auto entry = std::prev(entries.end());
      return {std::addressof(*entry), entry, false};
    }

    template <typename Binding, typename... Args>
    std::pair<entry_handle, typename Binding::container_type *>
    emplace_inline_binding_entry(
        typename rtti_type::type_index resolved_storage_type,
        std::optional<typename rtti_type::type_index> resolved_key_type,
        lookup_identity &&resolved_lookup_identity, Args &&...args) {
      if (!inline_entry_constructed_ && can_store_inline_binding<Binding>) {
        auto *instance =
            reinterpret_cast<Binding *>(inline_entry_binding_storage_);
        new (instance) Binding(std::forward<Args>(args)...);
        runtime_binding_ptr<runtime_binding_interface<container_type>> binding(
            instance, &registry_type::template destroy_inline_binding<Binding>);
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
        return {{inline_entry(), {}, true}, binding_container};
      }

      auto &&[binding, binding_container] =
          make_allocated_binding<Binding>(std::forward<Args>(args)...);
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

    bool insert_lookup_rows(entry_handle entry) {
      return lookup_table.insert(*entry);
    }

    void erase_lookup_rows(registered_binding_entry *entry) {
      lookup_table.erase(*entry);
    }

    allocator_type *allocator_;
    entry_list entries;
    lookup_table_type lookup_table;

  private:
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

    template <typename Binding>
    static constexpr bool can_store_inline_binding =
        sizeof(Binding) <= inline_binding_storage_size &&
        alignof(Binding) <= inline_binding_storage_alignment;

    template <typename Binding, typename... Args>
    std::pair<runtime_binding_ptr<runtime_binding_interface<container_type>>,
              typename Binding::container_type *>
    make_no_key_singular_binding(Args &&...args) {
      if constexpr (can_store_inline_binding<Binding>) {
        auto *instance = reinterpret_cast<Binding *>(inline_binding_storage_);
        new (instance) Binding(std::forward<Args>(args)...);
        return {runtime_binding_ptr<runtime_binding_interface<container_type>>(
                    instance,
                    &registry_type::template destroy_inline_binding<Binding>),
                &instance->get_container()};
      } else {
        auto alloc = allocator_traits::rebind<Binding>(*allocator_);
        auto *instance = allocator_traits::allocate(alloc, 1);
        try {
          allocator_traits::construct(alloc, instance,
                                      std::forward<Args>(args)...);
        } catch (...) {
          allocator_traits::deallocate(alloc, instance, 1);
          throw;
        }
        return {
            runtime_binding_ptr<runtime_binding_interface<container_type>>(
                instance, &registry_type::template destroy_binding<Binding>),
            &instance->get_container()};
      }
    }

    template <typename Binding, typename... Args>
    std::pair<runtime_binding_ptr<runtime_binding_interface<container_type>>,
              typename Binding::container_type *>
    make_allocated_binding(Args &&...args) {
      auto alloc = allocator_traits::rebind<Binding>(*allocator_);
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

    alignas(registered_binding_entry) std::byte
        no_key_singular_entry_storage_[sizeof(registered_binding_entry)];
    alignas(inline_binding_storage_alignment) std::byte
        inline_binding_storage_[inline_binding_storage_size];
    alignas(inline_binding_storage_alignment) std::byte
        inline_entry_binding_storage_[inline_binding_storage_size];
    alignas(registered_binding_entry) std::byte
        inline_entry_storage_[sizeof(registered_binding_entry)];
    bool no_key_singular_entry_constructed_ = false;
    bool inline_entry_constructed_ = false;
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

  template <typename Interface>
  static constexpr bool uses_no_key_singular_fast_path() {
    using entries = normalized_view_entries;
    using many_entry =
        detail::selected_no_key_lookup_entry_t<Interface, ::dingo::many,
                                               entries>;
    return std::is_void_v<many_entry>;
  }

  template <typename T, typename IdType>
  static constexpr bool has_explicit_collection_lookup(IdType &&) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;
    using entries =
        detail::normalize_lookup_definitions_t<view_definition_type>;

    if constexpr (is_none_v<std::decay_t<IdType>>) {
      using many_entry =
          detail::selected_no_key_lookup_entry_t<index_interface_type,
                                                 ::dingo::many, entries>;
      return !std::is_void_v<many_entry>;
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      using key_type = typename std::decay_t<IdType>::type;
      using many_entry = detail::selected_typed_key_lookup_entry_t<
          index_interface_type, key_type, ::dingo::many, entries>;
      return !std::is_void_v<many_entry>;
    } else {
      using index_key_type = std::decay_t<IdType>;
      using lookup_entry =
          detail::selected_lookup_entry_t<index_interface_type, index_key_type,
                                          entries>;
      return !std::is_void_v<lookup_entry> &&
             std::is_same_v<
                 typename lookup_entry_cardinality<lookup_entry>::type,
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

  template <typename Request, typename IdType = none_t>
  runtime_selection runtime_source_select(IdType &&id = IdType()) {
    using lookup_type = normalized_type_t<Request>;
    using exact_type =
        std::remove_cv_t<std::remove_reference_t<request_interface_t<Request>>>;
    auto *state = runtime_bindings_if_present();
    auto *data = [&]() {
      if (!state) {
        return static_cast<runtime_type_bindings *>(nullptr);
      }
      if constexpr (!is_none_v<std::decay_t<IdType>> &&
                    !detail::is_typed_key_v<IdType>) {
        return state->type_bindings.template get<lookup_type>();
      } else {
        return state->type_bindings.template get<exact_type>();
      }
    }();
    if constexpr (!std::is_same_v<lookup_type, exact_type>) {
      if (!data && state) {
        if constexpr (!is_none_v<std::decay_t<IdType>> &&
                      !detail::is_typed_key_v<IdType>) {
          data = state->type_bindings.template get<exact_type>();
        } else {
          data = state->type_bindings.template get<lookup_type>();
        }
      }
    }
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

  template <typename T> runtime_type_bindings *runtime_collection_bindings() {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using lookup_type = normalized_type_t<resolve_type>;

    auto *state = runtime_bindings_if_present();
    return state ? state->type_bindings.template get<lookup_type>() : nullptr;
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
           data.lookup_table
               .template for_each_no_key<index_interface_type, ::dingo::one>(
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

    return data.lookup_table
        .template for_each_typed_key<index_interface_type, Key, ::dingo::one>(
            [&](auto &entry) {
              fn(results, resolve_collection_type<resolve_type>(*entry.binding,
                                                                context));
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
        detail::selected_lookup_entry_t<index_interface_type, index_key_type,
                                        normalized_view_entries>;

    if constexpr (std::is_void_v<lookup_entry>) {
      (void)results;
      (void)context;
      (void)fn;
      return 0;
    } else {
      if constexpr (std::is_same_v<typename lookup_entry::cardinality,
                                   ::dingo::many>) {
        return data.lookup_table.template for_each_runtime_key<
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
    using entries =
        detail::normalize_lookup_definitions_t<view_definition_type>;
    using many_entry =
        detail::selected_no_key_lookup_entry_t<index_interface_type,
                                               ::dingo::many, entries>;
    using one_entry =
        detail::selected_no_key_lookup_entry_t<index_interface_type,
                                               ::dingo::one, entries>;

    if constexpr (std::is_void_v<many_entry>) {
      if constexpr (std::is_void_v<one_entry>) {
        return append_no_key_one_collection(data, results, context,
                                            std::forward<Fn>(fn));
      } else {
        (void)results;
        (void)context;
        (void)fn;
        return 0;
      }
    } else {
      return data.lookup_table
          .template for_each_no_key<index_interface_type, ::dingo::many>(
              [&](auto &entry) {
                fn(results, resolve_collection_type<resolve_type>(
                                *entry.binding, context));
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
    using entries =
        detail::normalize_lookup_definitions_t<view_definition_type>;
    using many_entry = detail::selected_typed_key_lookup_entry_t<
        index_interface_type, key_type, ::dingo::many, entries>;
    using one_entry = detail::selected_typed_key_lookup_entry_t<
        index_interface_type, key_type, ::dingo::one, entries>;

    if constexpr (std::is_void_v<many_entry>) {
      if constexpr (std::is_void_v<one_entry>) {
        return append_typed_key_one_collection<T, key_type>(
            data, results, context, std::forward<Fn>(fn));
      } else {
        (void)results;
        (void)context;
        (void)fn;
        return 0;
      }
    } else {
      return data.lookup_table.template for_each_typed_key<
          index_interface_type, key_type, ::dingo::many>([&](auto &entry) {
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
           data.lookup_table
               .template count_no_key<index_interface_type, ::dingo::one>();
  }

  template <typename T, typename Key>
  std::size_t count_typed_key_one_collection(runtime_type_bindings &data) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    using index_interface_type = normalized_type_t<resolve_type>;

    return data.lookup_table
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
        detail::selected_lookup_entry_t<index_interface_type, index_key_type,
                                        normalized_view_entries>;

    if constexpr (std::is_void_v<lookup_entry>) {
      return 0;
    } else {
      if constexpr (std::is_same_v<typename lookup_entry::cardinality,
                                   ::dingo::many>) {
        return data.lookup_table.template count_runtime_key<
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
    using entries =
        detail::normalize_lookup_definitions_t<view_definition_type>;
    using many_entry =
        detail::selected_no_key_lookup_entry_t<index_interface_type,
                                               ::dingo::many, entries>;
    using one_entry =
        detail::selected_no_key_lookup_entry_t<index_interface_type,
                                               ::dingo::one, entries>;

    if constexpr (std::is_void_v<many_entry>) {
      if constexpr (std::is_void_v<one_entry>) {
        return count_no_key_one_collection<T>(data);
      } else {
        return 0;
      }
    } else {
      return data.lookup_table
          .template count_no_key<index_interface_type, ::dingo::many>();
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
    using entries =
        detail::normalize_lookup_definitions_t<view_definition_type>;
    using many_entry = detail::selected_typed_key_lookup_entry_t<
        index_interface_type, key_type, ::dingo::many, entries>;
    using one_entry = detail::selected_typed_key_lookup_entry_t<
        index_interface_type, key_type, ::dingo::one, entries>;

    if constexpr (std::is_void_v<many_entry>) {
      if constexpr (std::is_void_v<one_entry>) {
        return count_typed_key_one_collection<T, key_type>(data);
      } else {
        return 0;
      }
    } else {
      return data.lookup_table.template count_typed_key<
          index_interface_type, key_type, ::dingo::many>();
    }
  }

  template <typename T, typename Fn, typename IdType>
  std::size_t append_runtime_collection(T &results, runtime_context &context,
                                        Fn &&fn, IdType id) {
    auto data = runtime_collection_bindings<T>();
    if (!data) {
      return 0;
    }

    if constexpr (!is_none_v<std::decay_t<IdType>> &&
                  !detail::is_typed_key_v<IdType>) {
      return append_runtime_keyed_collection(*data, results, context,
                                             std::forward<Fn>(fn), id);
    } else if constexpr (is_none_v<std::decay_t<IdType>>) {
      return append_no_key_runtime_collection(*data, results, context,
                                              std::forward<Fn>(fn));
    } else {
      return append_typed_key_runtime_collection(*data, results, context,
                                                 std::forward<Fn>(fn), id);
    }
  }

  template <typename T, typename IdType>
  std::size_t count_runtime_collection(IdType id) {
    auto data = runtime_collection_bindings<T>();
    if (!data) {
      return 0;
    }

    if constexpr (!is_none_v<std::decay_t<IdType>> &&
                  !detail::is_typed_key_v<IdType>) {
      return count_runtime_keyed_collection<T>(*data, id);
    } else if constexpr (is_none_v<std::decay_t<IdType>>) {
      return count_no_key_runtime_collection<T>(*data);
    } else {
      return count_typed_key_runtime_collection<T>(*data, id);
    }
  }

private:
  template <typename Request, typename IdType>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  runtime_selection select_runtime_binding(runtime_type_bindings *data,
                                           IdType &&id) {
    if constexpr (is_none_v<std::decay_t<IdType>>) {
      using index_interface_type = normalized_type_t<Request>;
      using entries =
          detail::normalize_lookup_definitions_t<view_definition_type>;
      using many_entry =
          detail::selected_no_key_lookup_entry_t<index_interface_type,
                                                 ::dingo::many, entries>;

      if constexpr (!std::is_void_v<many_entry>) {
        return select_no_key_lookup_table_binding<index_interface_type,
                                                  ::dingo::many>(data);
      } else {
        return select_no_key_lookup_table_binding<index_interface_type,
                                                  ::dingo::one>(data);
      }
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      using key_id_type = std::decay_t<IdType>;
      using key_type = typename key_id_type::type;
      using index_interface_type = normalized_type_t<Request>;
      using entries =
          detail::normalize_lookup_definitions_t<view_definition_type>;
      using one_entry = detail::selected_typed_key_lookup_entry_t<
          index_interface_type, key_type, ::dingo::one, entries>;
      using many_entry = detail::selected_typed_key_lookup_entry_t<
          index_interface_type, key_type, ::dingo::many, entries>;

      if constexpr (std::is_void_v<one_entry> && !std::is_void_v<many_entry>) {
        return select_typed_key_lookup_table_binding<index_interface_type,
                                                     key_type, ::dingo::many>(
            data);
      } else {
        return select_typed_key_lookup_table_binding<index_interface_type,
                                                     key_type, ::dingo::one>(
            data);
      }
    } else {
      using index_key_type = std::decay_t<IdType>;
      using index_interface_type = normalized_type_t<Request>;
      using lookup_entry =
          detail::selected_lookup_entry_t<index_interface_type, index_key_type,
                                          normalized_view_entries>;
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
        using index_cardinality = typename lookup_entry::cardinality;
        return select_runtime_key_lookup_table_binding<
            index_interface_type, index_key_type, index_cardinality,
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
        selected = data->lookup_table
                       .template find_singular_no_key<Interface, Cardinality>(
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
          data->lookup_table
              .template find_singular_typed_key<Interface, Key, Cardinality>(
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
      selected = data->lookup_table.template find_singular_runtime_key<
          Interface, Key, Cardinality, LookupEntry>(key_value, ambiguous);
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

  template <typename TypeInterface, typename TypeStorage, typename Binding,
            typename IdType, typename KeyIdType>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  void register_type_binding(Binding &&binding, IdType &&id, KeyIdType) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();

    auto pb =
        ensure_runtime_bindings().type_bindings.template insert<TypeInterface>(
            get_allocator());
    auto &data = pb.first;

    if constexpr (!is_none_v<std::decay_t<IdType>>) {
      auto resolved_key_type = [&]() {
        if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
          return std::optional<typename rtti_type::type_index>{};
        } else {
          return std::optional<typename rtti_type::type_index>{
              rtti_type::template get_type_index<std::decay_t<KeyIdType>>()};
        }
      }();
      auto resolved_storage_type =
          rtti_type::template get_type_index<typename TypeStorage::type>();
      using lookup_entry =
          detail::selected_lookup_entry_t<TypeInterface, IdType,
                                          normalized_view_entries>;
      static_assert(!std::is_void_v<lookup_entry>,
                    "keyed registration or request has no matching "
                    "dingo view definition for interface/key");
      using lookup_cardinality_type =
          typename lookup_entry_cardinality<lookup_entry>::type;
      auto lookup_id = lookup_table_type::template make_runtime_key_identity<
          TypeInterface, IdType, lookup_cardinality_type>(get_allocator(), id);
      auto inserted_entry = data.emplace_entry(
          std::forward<Binding>(binding), std::move(resolved_storage_type),
          std::move(resolved_key_type), std::move(lookup_id));
      commit_registration_entry<TypeInterface, TypeStorage>(
          data, inserted_entry, [&] {
            return detail::make_type_index_already_registered_exception<
                TypeInterface, typename TypeStorage::type, IdType>();
          });
    } else if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
      using entries = normalized_view_entries;
      using one_entry =
          detail::selected_no_key_lookup_entry_t<TypeInterface, ::dingo::one,
                                                 entries>;
      using many_entry =
          detail::selected_no_key_lookup_entry_t<TypeInterface, ::dingo::many,
                                                 entries>;

      if constexpr (!std::is_void_v<one_entry> || !std::is_void_v<many_entry>) {
        auto resolved_storage_type =
            rtti_type::template get_type_index<typename TypeStorage::type>();
        using lookup_cardinality_type =
            std::conditional_t<!std::is_void_v<one_entry>, ::dingo::one,
                               ::dingo::many>;
        auto lookup_id = lookup_table_type::template make_no_key_identity<
            TypeInterface, lookup_cardinality_type>();
        auto inserted_entry = data.emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::nullopt, std::move(lookup_id));
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_already_registered_exception<
                  TypeInterface, typename TypeStorage::type>();
            });
      } else {
        auto resolved_storage_type =
            rtti_type::template get_type_index<typename TypeStorage::type>();
        auto lookup_id =
            lookup_table_type::template make_no_key_identity<TypeInterface,
                                                             ::dingo::one>();
        auto inserted_entry = data.emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            std::nullopt, std::move(lookup_id));
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_already_registered_exception<
                  TypeInterface, typename TypeStorage::type>();
            });
      }
    } else {
      using typed_key_type = typename std::decay_t<KeyIdType>::type;
      using entries = normalized_view_entries;
      using one_entry = detail::selected_typed_key_lookup_entry_t<
          TypeInterface, typed_key_type, ::dingo::one, entries>;
      using many_entry = detail::selected_typed_key_lookup_entry_t<
          TypeInterface, typed_key_type, ::dingo::many, entries>;

      if constexpr (!std::is_void_v<one_entry> || !std::is_void_v<many_entry>) {
        auto resolved_storage_type =
            rtti_type::template get_type_index<typename TypeStorage::type>();
        using lookup_cardinality_type =
            std::conditional_t<!std::is_void_v<one_entry>, ::dingo::one,
                               ::dingo::many>;
        auto lookup_id = lookup_table_type::template make_typed_key_identity<
            TypeInterface, typed_key_type, lookup_cardinality_type>();
        auto inserted_entry = data.emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            rtti_type::template get_type_index<std::decay_t<KeyIdType>>(),
            std::move(lookup_id));
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_index_already_registered_exception<
                  TypeInterface, typename TypeStorage::type,
                  std::decay_t<KeyIdType>>();
            });
      } else {
        auto resolved_storage_type =
            rtti_type::template get_type_index<typename TypeStorage::type>();
        auto lookup_id = lookup_table_type::template make_typed_key_identity<
            TypeInterface, typed_key_type, ::dingo::one>();
        auto inserted_entry = data.emplace_entry(
            std::forward<Binding>(binding), std::move(resolved_storage_type),
            rtti_type::template get_type_index<std::decay_t<KeyIdType>>(),
            std::move(lookup_id));
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_index_already_registered_exception<
                  TypeInterface, typename TypeStorage::type,
                  std::decay_t<KeyIdType>>();
            });
      }
    }

    runtime_bindings_present_ = true;
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

  template <typename TypeInterface, typename TypeStorage, typename Binding,
            typename Parent, typename IdType, typename KeyIdType,
            typename... Args>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  typename Binding::container_type &
  register_constructed_type_binding(Parent *parent, IdType &&id, KeyIdType,
                                    Args &&...args) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();

    auto &data =
        ensure_runtime_bindings()
            .type_bindings.template insert<TypeInterface>(get_allocator())
            .first;

    if constexpr (!is_none_v<std::decay_t<IdType>>) {
      auto resolved_key_type = [&]() {
        if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
          return std::optional<typename rtti_type::type_index>{};
        } else {
          return std::optional<typename rtti_type::type_index>{
              rtti_type::template get_type_index<std::decay_t<KeyIdType>>()};
        }
      }();
      auto resolved_storage_type =
          rtti_type::template get_type_index<typename TypeStorage::type>();
      using lookup_entry =
          detail::selected_lookup_entry_t<TypeInterface, IdType,
                                          normalized_view_entries>;
      static_assert(!std::is_void_v<lookup_entry>,
                    "keyed registration or request has no matching "
                    "dingo view definition for interface/key");
      using lookup_cardinality_type =
          typename lookup_entry_cardinality<lookup_entry>::type;
      auto lookup_id = lookup_table_type::template make_runtime_key_identity<
          TypeInterface, IdType, lookup_cardinality_type>(get_allocator(), id);
      auto &&[inserted_entry, binding_container] =
          data.template emplace_inline_binding_entry<Binding>(
              std::move(resolved_storage_type), std::move(resolved_key_type),
              std::move(lookup_id), parent, std::forward<Args>(args)...);
      commit_registration_entry<TypeInterface, TypeStorage>(
          data, inserted_entry, [&] {
            return detail::make_type_index_already_registered_exception<
                TypeInterface, typename TypeStorage::type, IdType>();
          });
      runtime_bindings_present_ = true;
      return *binding_container;
    } else if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
      using entries = normalized_view_entries;
      using one_entry =
          detail::selected_no_key_lookup_entry_t<TypeInterface, ::dingo::one,
                                                 entries>;
      using many_entry =
          detail::selected_no_key_lookup_entry_t<TypeInterface, ::dingo::many,
                                                 entries>;

      auto resolved_storage_type =
          rtti_type::template get_type_index<typename TypeStorage::type>();
      if constexpr (!std::is_void_v<one_entry> || !std::is_void_v<many_entry>) {
        using lookup_cardinality_type =
            std::conditional_t<!std::is_void_v<one_entry>, ::dingo::one,
                               ::dingo::many>;
        auto lookup_id = lookup_table_type::template make_no_key_identity<
            TypeInterface, lookup_cardinality_type>();
        auto &&[inserted_entry, binding_container] =
            data.template emplace_inline_binding_entry<Binding>(
                std::move(resolved_storage_type), std::nullopt,
                std::move(lookup_id), parent, std::forward<Args>(args)...);
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_already_registered_exception<
                  TypeInterface, typename TypeStorage::type>();
            });
        runtime_bindings_present_ = true;
        return *binding_container;
      } else {
        auto lookup_id =
            lookup_table_type::template make_no_key_identity<TypeInterface,
                                                             ::dingo::one>();
        auto &&[inserted_entry, binding_container] =
            data.template emplace_inline_binding_entry<Binding>(
                std::move(resolved_storage_type), std::nullopt,
                std::move(lookup_id), parent, std::forward<Args>(args)...);
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_already_registered_exception<
                  TypeInterface, typename TypeStorage::type>();
            });
        runtime_bindings_present_ = true;
        return *binding_container;
      }
    } else {
      using typed_key_type = typename std::decay_t<KeyIdType>::type;
      using entries = normalized_view_entries;
      using one_entry = detail::selected_typed_key_lookup_entry_t<
          TypeInterface, typed_key_type, ::dingo::one, entries>;
      using many_entry = detail::selected_typed_key_lookup_entry_t<
          TypeInterface, typed_key_type, ::dingo::many, entries>;

      auto resolved_storage_type =
          rtti_type::template get_type_index<typename TypeStorage::type>();
      auto resolved_key_type =
          rtti_type::template get_type_index<std::decay_t<KeyIdType>>();
      if constexpr (!std::is_void_v<one_entry> || !std::is_void_v<many_entry>) {
        using lookup_cardinality_type =
            std::conditional_t<!std::is_void_v<one_entry>, ::dingo::one,
                               ::dingo::many>;
        auto lookup_id = lookup_table_type::template make_typed_key_identity<
            TypeInterface, typed_key_type, lookup_cardinality_type>();
        auto &&[inserted_entry, binding_container] =
            data.template emplace_inline_binding_entry<Binding>(
                std::move(resolved_storage_type), std::move(resolved_key_type),
                std::move(lookup_id), parent, std::forward<Args>(args)...);
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_index_already_registered_exception<
                  TypeInterface, typename TypeStorage::type,
                  std::decay_t<KeyIdType>>();
            });
        runtime_bindings_present_ = true;
        return *binding_container;
      } else {
        auto lookup_id = lookup_table_type::template make_typed_key_identity<
            TypeInterface, typed_key_type, ::dingo::one>();
        auto &&[inserted_entry, binding_container] =
            data.template emplace_inline_binding_entry<Binding>(
                std::move(resolved_storage_type), std::move(resolved_key_type),
                std::move(lookup_id), parent, std::forward<Args>(args)...);
        commit_registration_entry<TypeInterface, TypeStorage>(
            data, inserted_entry, [&] {
              return detail::make_type_index_already_registered_exception<
                  TypeInterface, typename TypeStorage::type,
                  std::decay_t<KeyIdType>>();
            });
        runtime_bindings_present_ = true;
        return *binding_container;
      }
    }
  }

  template <typename TypeInterface, typename TypeStorage, typename Binding,
            typename... Args>
  typename Binding::container_type &
  register_no_key_singular_binding(Args &&...args) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();

    auto &state = ensure_runtime_bindings();
    auto pb =
        state.type_bindings.template insert<TypeInterface>(get_allocator());
    auto &data = pb.first;

    bool type_bindings_inserted = pb.second;
    try {
      bool ambiguous = false;
      auto *selected =
          data.lookup_table
              .template find_singular_no_key<TypeInterface, ::dingo::one>(
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
        state.type_bindings.template erase<TypeInterface>();
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

  alignas(runtime_bindings_state) std::byte
      runtime_bindings_[sizeof(runtime_bindings_state)];
  bool runtime_bindings_constructed_ = false;

  bool runtime_bindings_present_ = false;
};

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
auto runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::runtime_bindings_if_present()
    -> runtime_bindings_state * {
  if (!runtime_bindings_constructed_) {
    return nullptr;
  }
  return std::launder(
      reinterpret_cast<runtime_bindings_state *>(runtime_bindings_));
}

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
auto runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::runtime_bindings_if_present() const
    -> const runtime_bindings_state * {
  if (!runtime_bindings_constructed_) {
    return nullptr;
  }
  return std::launder(
      reinterpret_cast<const runtime_bindings_state *>(runtime_bindings_));
}

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
auto runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::ensure_runtime_bindings()
    -> runtime_bindings_state & {
  if (!runtime_bindings_constructed_) {
    new (runtime_bindings_) runtime_bindings_state(get_allocator());
    runtime_bindings_constructed_ = true;
  }
  return *std::launder(
      reinterpret_cast<runtime_bindings_state *>(runtime_bindings_));
}

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
void runtime_registry<ContainerTraits, Allocator, ParentRegistry,
                      ResolveRoot>::destroy_runtime_bindings() {
  if (runtime_bindings_constructed_) {
    std::launder(reinterpret_cast<runtime_bindings_state *>(runtime_bindings_))
        ->~runtime_bindings_state();
    runtime_bindings_constructed_ = false;
  }
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
