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
#include <dingo/index/index.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/registration/requirements.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/static/local_resolution.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/request_traits.h>
#include <dingo/type/type_list.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <type_traits>
#include <typeindex>
#include <variant>

namespace dingo {
namespace detail {

template <typename StaticRegistry, typename ParentContainer>
class container_with_static_bindings;

} // namespace detail

template <typename ContainerTraits, typename Allocator, typename ParentRegistry,
          typename ResolveRoot>
class runtime_registry : public allocator_base<Allocator> {
  friend class runtime_context;
  template <typename, typename> friend class detail::binding_resolution;
  template <typename, typename>
  friend class detail::container_with_static_bindings;
  template <typename, typename, typename> friend class runtime_container;
  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentRegistryT, typename ResolveRootT>
  friend class runtime_registry;

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
  using index_definition_type = typename ContainerTraits::index_definition_type;
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
              if constexpr (detail::is_typed_key_v<IdType>) {
                registered_binding_entry *candidate = nullptr;
                for (auto &&p : data->bindings) {
                  auto &entry = p.second;
                  if (!entry.key_type ||
                      !(*entry.key_type == rtti_type::template get_type_index<
                                               std::decay_t<IdType>>())) {
                    continue;
                  }

                  if (candidate) {
                    candidate = nullptr;
                    break;
                  }
                  candidate = &entry;
                }

                if (candidate && candidate->cache) {
                  return detail::convert_resolved_binding<
                      request_interface_t<T>>(candidate->cache);
                }
              } else {
                using index_entry = detail::selected_index_entry_t<
                    lookup_type, IdType,
                    detail::normalize_index_definitions_t<
                        index_definition_type>>;
                if constexpr (std::is_void_v<index_entry> &&
                              !std::is_same_v<void, ParentRegistry> &&
                              !std::is_base_of_v<registry_type,
                                                 resolve_root_type>) {
                  (void)data;
                } else {
                  auto indexed = data->template get_index<lookup_type, IdType>(
                                         get_allocator())
                                     .find(id);

                  if (indexed) {
                    if (indexed->cache) {
                      return detail::convert_resolved_binding<
                          request_interface_t<T>>(indexed->cache);
                    }
                  }
                }
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
    if (count == 0) {
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

  struct index_data : binding_cache_state {
    runtime_binding_interface<container_type> *binding = nullptr;

    index_data() = default;

    explicit index_data(runtime_binding_interface<container_type> *binding_ptr)
        : binding(binding_ptr) {}

    operator bool() const { return binding != nullptr; }
  };

  struct registered_binding_entry : binding_cache_state {
    runtime_binding_ptr<runtime_binding_interface<container_type>> binding;
    std::optional<typename rtti_type::type_index> key_type;

    registered_binding_entry(
        runtime_binding_ptr<runtime_binding_interface<container_type>>
            &&binding_ptr,
        std::optional<typename rtti_type::type_index> resolved_key_type =
            std::nullopt)
        : binding(std::move(binding_ptr)),
          key_type(std::move(resolved_key_type)) {}

    registered_binding_entry(const registered_binding_entry &) = delete;
    registered_binding_entry &
    operator=(const registered_binding_entry &) = delete;

    registered_binding_entry(registered_binding_entry &&other) noexcept
        : binding_cache_state{other.cache}, binding(std::move(other.binding)),
          key_type(std::move(other.key_type)) {
      other.cache = nullptr;
    }

    registered_binding_entry &
    operator=(registered_binding_entry &&other) noexcept {
      if (this != &other) {
        this->cache = other.cache;
        other.cache = nullptr;
        binding = std::move(other.binding);
        key_type = std::move(other.key_type);
      }
      return *this;
    }
  };

  using index_bindings_type =
      detail::index_bindings<index_definition_type, index_data, allocator_type>;

  struct runtime_type_bindings : index_bindings_type {
    runtime_type_bindings(allocator_type &allocator)
        : index_bindings_type(allocator), bindings(allocator) {}

    typename ContainerTraits::template type_map_type<registered_binding_entry,
                                                     allocator_type>
        bindings;
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
  template <typename Key>
  using collection_key_t =
      std::conditional_t<std::is_void_v<Key>, none_t, key<Key>>;

protected:
  template <typename Key> static collection_key_t<Key> collection_key() {
    return {};
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

  template <typename Entry>
  static bool runtime_collection_entry_matches(const Entry &, none_t) {
    return true;
  }

  template <typename Entry, typename Key>
  static bool runtime_collection_entry_matches(const Entry &entry, key<Key>) {
    return entry.key_type &&
           *entry.key_type == rtti_type::template get_type_index<key<Key>>();
  }

  template <typename T, typename Fn, typename IdType>
  std::size_t append_runtime_collection(T &results, runtime_context &context,
                                        Fn &&fn, IdType id) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    auto data = runtime_collection_bindings<T>();
    if (!data) {
      return 0;
    }

    std::size_t count = 0;
    for (auto &&p : data->bindings) {
      auto &entry = p.second;
      if (!runtime_collection_entry_matches(entry, id)) {
        continue;
      }
      ++count;
      fn(results,
         resolve_collection_type<resolve_type>(*entry.binding, context));
    }

    return count;
  }

  template <typename T, typename IdType>
  std::size_t count_runtime_collection(IdType id) {
    auto data = runtime_collection_bindings<T>();
    if (!data) {
      return 0;
    }

    if constexpr (is_none_v<std::decay_t<IdType>>) {
      return data->bindings.size();
    }

    std::size_t count = 0;
    for (auto &&p : data->bindings) {
      if (runtime_collection_entry_matches(p.second, id)) {
        ++count;
      }
    }
    return count;
  }

private:
  template <typename Request, typename IdType>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  runtime_selection select_runtime_binding(runtime_type_bindings *data,
                                           IdType &&id) {
    if constexpr (is_none_v<std::decay_t<IdType>>) {
      return detail::make_runtime_selection<runtime_binding_interface_type,
                                            binding_cache_state *>(
          [&](auto &&select) {
            if (!data) {
              return;
            }

            for (auto &&p : data->bindings) {
              auto &entry = p.second;
              select(*entry.binding, &entry);
            }
          });
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      return detail::make_runtime_selection<
          runtime_binding_interface_type,
          binding_cache_state *>([&](auto &&select) {
        if (!data) {
          return;
        }

        for (auto &&p : data->bindings) {
          auto &entry = p.second;
          if (!entry.key_type ||
              !(*entry.key_type ==
                rtti_type::template get_type_index<std::decay_t<IdType>>())) {
            continue;
          }

          select(*entry.binding, &entry);
        }
      });
    } else {
      using index_key_type = std::decay_t<IdType>;
      using index_interface_type = normalized_type_t<Request>;
      using index_entry = detail::selected_index_entry_t<
          index_interface_type, index_key_type,
          detail::normalize_index_definitions_t<index_definition_type>>;
      if constexpr (std::is_void_v<index_entry> &&
                    !std::is_same_v<void, ParentRegistry> &&
                    !std::is_base_of_v<registry_type, resolve_root_type>) {
        return detail::make_runtime_selection<runtime_binding_interface_type,
                                              binding_cache_state *>(nullptr,
                                                                     nullptr);
      } else {
        auto indexed =
            data ? data->template get_index<index_interface_type,
                                            index_key_type>(get_allocator())
                       .find(id)
                 : nullptr;
        return detail::make_runtime_selection<runtime_binding_interface_type,
                                              binding_cache_state *>(
            indexed ? indexed->binding : nullptr, indexed);
      }
    }
  }

  template <typename... TypeArgs, typename Parent, typename Arg,
            typename IdType>
  auto &register_type_impl(Parent *parent, Arg &&arg, IdType &&id) {
    static_assert(!detail::has_explicit_void_interface_v<TypeArgs...>,
                  "interfaces<void> is not a valid registration target");
    using registration =
        std::conditional_t<!is_none_v<std::decay_t<Arg>>,
                           type_registration<TypeArgs..., factory<Arg>>,
                           type_registration<TypeArgs...>>;
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
          auto &&[binding, binding_container] =
              allocate_binding<registered_binding_type>(parent,
                                                        std::forward<Arg>(arg));
          register_type_binding<interface_type, storage_type>(
              std::move(binding), std::forward<IdType>(id), key_id_type{});
          return *binding_container;
        } else {
          auto &&[binding, binding_container] =
              allocate_binding<registered_binding_type>(parent);
          register_type_binding<interface_type, storage_type>(
              std::move(binding), std::forward<IdType>(id), key_id_type{});
          return *binding_container;
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
  void register_type_binding(Binding &&binding, IdType &&id, KeyIdType) {
    check_interface_requirements<TypeStorage,
                                 typename annotated_traits<TypeInterface>::type,
                                 typename TypeStorage::type>();

    auto pb =
        ensure_runtime_bindings().type_bindings.template insert<TypeInterface>(
            get_allocator());
    auto &data = pb.first;
    using binding_registration_key =
        std::conditional_t<is_none_v<std::decay_t<KeyIdType>>,
                           type_list<TypeInterface, typename TypeStorage::type>,
                           type_list<TypeInterface, typename TypeStorage::type,
                                     std::decay_t<KeyIdType>>>;

    auto inserted_binding = [&]() {
      if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
        return data.bindings.template insert<binding_registration_key>(
            std::forward<Binding>(binding), std::nullopt);
      } else {
        return data.bindings.template insert<binding_registration_key>(
            std::forward<Binding>(binding),
            rtti_type::template get_type_index<std::decay_t<KeyIdType>>());
      }
    }();
    if (!inserted_binding.second) {
      if constexpr (is_none_v<std::decay_t<KeyIdType>>) {
        throw detail::make_type_already_registered_exception<
            TypeInterface, typename TypeStorage::type>();
      } else {
        throw detail::make_type_index_already_registered_exception<
            TypeInterface, typename TypeStorage::type,
            std::decay_t<KeyIdType>>();
      }
    }
    auto binding_ptr = inserted_binding.first.binding.get();
    runtime_bindings_present_ = true;

    if constexpr (!is_none_v<std::decay_t<IdType>>) {
      if (!data.template get_index<TypeInterface, IdType>(get_allocator())
               .emplace(std::forward<IdType>(id), index_data{binding_ptr})) {
        bool erased = data.bindings.template erase<binding_registration_key>();
        assert(erased);
        (void)erased;
        throw detail::make_type_index_already_registered_exception<
            TypeInterface, typename TypeStorage::type, IdType>();
      }
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
  T resolve(Binding &binding, Context &context, index_data &data) {
    return ::dingo::resolve_binding_request<T, rtti_type>(
        binding, context,
        cache_enabled
            ? instance_cache_sink{&data, &registry_type::store_index_cache}
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

} // namespace dingo
