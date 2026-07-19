//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/auto_constructible.h>
#include <dingo/core/binding_collection.h>
#include <dingo/core/binding_model.h>
#include <dingo/core/binding_resolution_policy.h>
#include <dingo/core/binding_selection.h>
#include <dingo/core/config.h>
#include <dingo/core/dependency.h>
#include <dingo/core/exceptions.h>
#include <dingo/core/none.h>
#include <dingo/detail/container_base.h>
#include <dingo/detail/container_traits.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/annotated.h>
#include <dingo/registration/collection_traits.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/resolution/runtime_binding.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime_container.h>
#include <dingo/static/container_traits.h>
#include <dingo/static/local_resolution.h>
#include <dingo/static/resolution.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/complete_type.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/normalized_type.h>

#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {
namespace detail {

template <typename Self, typename RuntimeConfig>
struct static_container_runtime_configuration {
  using runtime_base =
      runtime_registry<typename RuntimeConfig::container_traits_type,
                       typename RuntimeConfig::allocator_type,
                       typename RuntimeConfig::parent_registry_type,
                       typename RuntimeConfig::resolve_root_type,
                       RuntimeConfig::owns_runtime_data>;

  static constexpr bool owns_runtime_data = RuntimeConfig::owns_runtime_data;
  static constexpr bool merge_parent_collections =
      RuntimeConfig::merge_parent_collections;
};

template <typename Self>
struct static_container_runtime_configuration<Self, void> {
  using runtime_base =
      runtime_registry<dynamic_container_traits,
                       typename dynamic_container_traits::allocator_type, Self,
                       Self>;

  static constexpr bool owns_runtime_data = true;
  static constexpr bool merge_parent_collections = false;
};

} // namespace detail

template <typename ParentContainer, typename RuntimeConfig,
          typename... Registrations>
class detail::container_with_static_bindings<static_bindings<Registrations...>,
                                             ParentContainer, RuntimeConfig>
    : public detail::runtime_registration_api<
          detail::container_with_static_bindings<
              static_bindings<Registrations...>, ParentContainer,
              RuntimeConfig>> {
  template <typename> friend class runtime_context;
  template <typename, typename, typename>
  friend class detail::container_with_static_bindings;

public:
  using static_bindings_type = static_bindings<Registrations...>;
  using static_registry_type =
      detail::static_registry<static_bindings_type,
                              detail::static_storage_state<Registrations...>>;

private:
  using self_type =
      detail::container_with_static_bindings<static_bindings_type,
                                             ParentContainer, RuntimeConfig>;
  using runtime_configuration =
      detail::static_container_runtime_configuration<self_type, RuntimeConfig>;
  using runtime_base = typename runtime_configuration::runtime_base;
  using runtime_context_type =
      runtime_context<typename runtime_base::allocator_type>;
  friend runtime_base;
  template <typename, typename, typename, typename, bool>
  friend class ::dingo::runtime_registry;

  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<
          typename runtime_base::container_traits_type>>;
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using parent_container_type = ParentContainer;

  template <typename T, typename Key>
  using static_selection_t = detail::static_binding_t<
      typename static_bindings_type::template bindings<T, Key>>;

  template <typename T>
  static constexpr bool runtime_auto_constructible_v =
      std::is_same_v<typename request_type<T>::value_type, std::decay_t<T>> &&
      (!std::is_reference_v<T> ||
       (std::is_lvalue_reference_v<T> &&
        std::is_const_v<std::remove_reference_t<T>> &&
        is_auto_constructible<std::decay_t<T>>::value));

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_binding(LookupKey key) {
    return runtime_registry_.template binding_status<Request>(key) !=
           detail::binding_status::not_found;
  }

  template <typename Request, typename NormalizedRequest>
  bool has_runtime_no_key_binding() {
    auto key = detail::no_lookup_key();
    static_assert(
        std::is_same_v<typename request_type<Request>::value_type,
                       typename request_type<NormalizedRequest>::value_type>);
    return has_runtime_binding<Request>(key);
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_collection(LookupKey key) {
    return runtime_registry_.template count_collection<T>(key) != 0;
  }

  template <typename Request, typename Key>
  static constexpr detail::binding_status static_resolve_status_v =
      static_selection_t<typename Request::lookup_type, Key>::status;

  template <typename Request, typename Key,
            bool Found = static_selection_t<Request, Key>::status ==
                         detail::binding_status::found>
  struct is_static_binding_available : std::false_type {};

  template <typename Request, typename Key>
  struct is_static_binding_available<Request, Key, true> {
    using binding = typename static_selection_t<Request, Key>::binding_type;
    static constexpr bool value = detail::binding_supports_request_v<
        typename request_type<Request>::interface_type, binding>;
  };

  template <typename Request, typename Key>
  static constexpr bool is_static_binding_available_v =
      is_static_binding_available<Request, Key>::value;
  template <typename T>
  static constexpr bool has_static_construct_v = [] {
    using request = request_type<T>;
    using interface_type = typename request::interface_type;
    using value_type = typename request::value_type;
    using no_key = detail::no_lookup_key_t;
    constexpr bool has_exact_static_binding =
        is_static_binding_available_v<interface_type, no_key>;
    constexpr bool has_normalized_static_binding =
        is_static_binding_available_v<value_type, no_key>;

    return has_exact_static_binding ||
           (has_normalized_static_binding && construct_normalized_request_v<T>);
  }();

  template <typename T, typename Key>
  bool select_static_collection_construct() {
    constexpr bool has_static_collection_bindings =
        detail::static_collection_binding_count<static_bindings_type, T,
                                                Key>() != 0;
    if constexpr (!has_static_collection_bindings) {
      return false;
    } else {
      return !has_runtime_collection<T>(Key{});
    }
  }

  template <typename Collection, typename Key>
  static constexpr bool has_static_collection_v =
      detail::static_collection_binding_count<static_bindings_type, Collection,
                                              Key>() != 0 &&
      detail::static_bindings_resolvable_v<
          typename static_bindings_type::template bindings<
              normalized_type_t<
                  typename collection_traits<Collection>::resolve_type>,
              Key>,
          static_bindings_type>;

  template <typename Request, typename R = typename Request::result_type>
  DINGO_ALWAYS_INLINE R construct_static(construction_scope scope,
                                         runtime_context_type &context) {
    using user_type = typename Request::user_type;
    using interface_type = typename Request::interface_type;
    using request_value_type = typename Request::value_type;
    using no_key = detail::no_lookup_key_t;
    constexpr bool has_exact_static_binding =
        is_static_binding_available_v<interface_type, no_key>;
    constexpr bool has_normalized_static_binding =
        is_static_binding_available_v<request_value_type, no_key>;
    if constexpr (has_exact_static_binding) {
      using binding =
          typename static_selection_t<interface_type, no_key>::binding_type;
      if constexpr (detail::binding_supports_request_v<user_type, binding>) {
        return resolve_static<Request, no_key>(scope, context);
      }
    }

    if constexpr (has_normalized_static_binding &&
                  construct_normalized_request_v<user_type>) {
      using normalized_selection =
          static_selection_t<request_value_type, no_key>;
      return detail::construct_static_binding_value<user_type,
                                                    normalized_selection>(
          [&]() {
            return resolve_static<request_type<request_value_type>, no_key>(
                scope, context);
          });
    } else {
      return resolve_static<Request, no_key>(scope, context);
    }
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_impl(construction_scope scope,
                              runtime_context_type &context, LookupKey key,
                              Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    const std::size_t total = count_collection<T>(key);
    // A declared collection lookup with no rows is a valid empty collection.
    if (total == 0 &&
        !runtime_registry_.template has_explicit_collection_lookup<T>(key)) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    T results;
    collection_type::reserve(results, total);

    auto &&append = fn;
    append_collection_impl<T>(scope, results, context, key, append);
    return results;
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
    return construct_collection_impl<T>(scope, context, std::move(key),
                                        std::forward<Fn>(fn));
  }

  template <typename Request, typename LookupKey, typename Context,
            typename R = typename Request::result_type>
  DINGO_ALWAYS_INLINE R resolve_static(construction_scope scope,
                                       Context &context) {
    return static_registry_.template resolve_request<Request, LookupKey>(
        scope, context, *this);
  }

  template <typename Request, typename Origin, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(construction_scope scope, runtime_context_type &context,
                   Origin &origin, LookupKey key) {
    using user_type = typename Request::user_type;
    return parent_
        ->template resolve<user_type, Request::removes_rvalue_references>(
            scope, context, origin, std::move(key));
  }

  template <typename Request, typename R = typename Request::lookup_type>
  R resolve_request(construction_scope scope, runtime_context_type &context) {
    auto key = detail::no_lookup_key();
    auto selection =
        runtime_registry_
            .template select_binding<typename Request::lookup_type>(key);
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename Request::lookup_type>(context);
    }
    if (selection.status == detail::binding_status::found) {
      return runtime_registry_
          .template resolve_binding<typename Request::interface_type, R>(
              selection, scope, context);
    }
    return unresolved<Request,
                      runtime_auto_constructible_v<typename Request::user_type>,
                      decltype(key), R>(scope, context, std::move(key));
  }

  template <typename Request, typename R = typename Request::result_type>
  R construct_runtime_resolved_request(construction_scope scope,
                                       runtime_context_type &context) {
    try {
      return resolve_request<Request>(scope, context);
    } catch (const type_not_convertible_exception &) {
      using request_value_type = typename Request::value_type;
      auto &&value =
          resolve_request<request_type<request_value_type>>(scope, context);
      return type_traits<std::decay_t<typename Request::user_type>>::make(
          std::forward<decltype(value)>(value));
    }
  }

  template <typename Request,
            typename Factory = constructor<typename Request::value_type>,
            typename R = typename Request::result_type>
  R construct_runtime_request(construction_scope scope,
                              runtime_context_type &context,
                              Factory factory = Factory()) {
    using user_type = typename Request::user_type;
    using request_value_type = typename Request::value_type;

    if constexpr (std::is_same_v<Factory, constructor<request_value_type>>) {
      auto key = detail::no_lookup_key();
      if (runtime_registry_
              .template binding_status<typename Request::lookup_type>(key) !=
          detail::binding_status::not_found) {
        if constexpr (!construct_normalized_request_v<user_type> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          user_type>) {
          return resolve_request<Request>(scope, context);
        } else {
          return construct_runtime_resolved_request<Request, R>(scope, context);
        }
      }
    }

    if constexpr (construct_factory_request_v<user_type>) {
      auto type_guard = context.template track_type<request_value_type>();
      return factory.template construct<R>(scope, context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             user_type>) {
      ::dingo::throw_missing_rvalue_conversion<user_type>(false, context);
    } else {
      return resolve_request<Request>(scope, context);
    }
  }

public:
  using runtime_registry_type = runtime_base;
  using container_traits_type = typename runtime_base::container_traits_type;
  using allocator_type = typename runtime_base::allocator_type;
  using rtti_type = typename runtime_base::rtti_type;

  static_assert(static_bindings_type::valid,
                "container requires a valid compile-time bindings source");
  static_assert(detail::key_value_bindings_are_declared<
                    typename static_bindings_type::interface_bindings,
                    index_entries_>::value,
                "container fixed dingo::key_type<Key, Value> bindings require "
                "static_container with associative<Key, Interface, one> or "
                "associative<Key, Interface, many>");
  static_assert(detail::key_value_bindings_are_unique<
                    typename static_bindings_type::interface_bindings,
                    index_entries_>::value,
                "container fixed runtime-key lookup bindings must be unique "
                "for one lookups and unique by storage for many lookups");
  static_assert(
      std::is_void_v<RuntimeConfig> ||
          detail::graph_analysis<static_bindings_type, true>::resolvable,
      "register_type bindings<...> requires a resolvable compile-time binding "
      "graph");
  static_assert(
      !std::is_void_v<RuntimeConfig> ||
          detail::graph_analysis<static_bindings_type, true>::resolvable,
      "container requires a resolvable compile-time binding graph");
  static_assert(
      (detail::binding_factory_is_default_constructible<
           detail::binding_model<Registrations>>::value &&
       ...),
      "container requires default-constructible compile-time factories");
  static_assert((detail::binding_storage_is_default_constructible<
                     detail::binding_model<Registrations>>::value &&
                 ...),
                "container requires default-constructible compile-time "
                "storage objects");

  template <bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<OwnsRuntimeData, int> = 0>
  container_with_static_bindings()
      : runtime_registry_(detail::runtime_data_owner, this) {}

  template <bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<OwnsRuntimeData, int> = 0>
  explicit container_with_static_bindings(allocator_type alloc)
      : runtime_registry_(detail::runtime_data_owner, this, alloc) {}

  template <
      typename Parent = ParentContainer,
      bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
      std::enable_if_t<!std::is_void_v<Parent> && OwnsRuntimeData, int> = 0>
  explicit container_with_static_bindings(
      Parent *parent, allocator_type alloc = allocator_type())
      : runtime_registry_(detail::runtime_data_owner, this, alloc),
        parent_(parent) {}

  template <
      typename Parent = ParentContainer,
      bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
      std::enable_if_t<!std::is_void_v<Parent> && !OwnsRuntimeData, int> = 0>
  explicit container_with_static_bindings(
      Parent *parent, allocator_type alloc = allocator_type())
      : runtime_registry_(parent, alloc), parent_(parent) {}

  container_with_static_bindings(const self_type &other)
      : runtime_registry_(other.runtime_registry_),
        static_registry_(other.static_registry_), parent_(other.parent_) {}

  container_with_static_bindings(self_type &&other) noexcept
      : runtime_registry_(std::move(other.runtime_registry_)),
        static_registry_(std::move(other.static_registry_)),
        parent_(other.parent_) {}

  self_type &operator=(const self_type &other) {
    if (this != &other) {
      runtime_registry_ = other.runtime_registry_;
      static_registry_ = other.static_registry_;
      parent_ = other.parent_;
    }
    return *this;
  }

  self_type &operator=(self_type &&other) noexcept {
    if (this != &other) {
      runtime_registry_ = std::move(other.runtime_registry_);
      static_registry_ = std::move(other.static_registry_);
      parent_ = other.parent_;
    }
    return *this;
  }

  self_type &container() { return *this; }

  const self_type &container() const { return *this; }

private:
  template <typename Request, typename R, typename LookupKey>
  static constexpr bool is_cacheable() {
    if constexpr (!detail::cache::supports_v<
                      typename Request::interface_type> ||
                  collection_traits<R>::is_collection) {
      return false;
    } else if constexpr (detail::is_static_lookup_key_definition_v<LookupKey>) {
      using static_selection =
          typename static_registry_type::template selection<
              typename Request::lookup_type, LookupKey>;
      return static_selection::status == detail::binding_status::not_found;
    } else {
      return true;
    }
  }

  template <typename Request, typename R>
  DINGO_NOINLINE R resolve_selected(
      typename runtime_registry_type::runtime_selection selection) {
    using interface_type = typename Request::interface_type;
    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
          return runtime_registry_.template resolve_binding<interface_type, R>(
              selection, ephemeral_scope, context);
        });
  }

  template <typename Request, typename R, typename LookupKey>
  R resolve_entry(LookupKey key) {
    using interface_type = typename Request::interface_type;
    if constexpr (is_cacheable<Request, R, LookupKey>()) {
      auto selection =
          runtime_registry_
              .template select_binding<typename Request::lookup_type>(key);
      auto result =
          runtime_registry_.template lookup_cache<interface_type>(selection);
      if (result.hit) {
        return runtime_registry_.template resolve_cached<interface_type, R>(
            result);
      }
      if (selection.status == detail::binding_status::found) {
        return resolve_selected<Request, R>(selection);
      }
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            return resolve<Request, R>(selection, ephemeral_scope, context,
                                       *this, std::move(key));
          });
    }

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
          return resolve<Request, R>(ephemeral_scope, context, *this,
                                     std::move(key));
        });
  }

public:
  template <typename T, typename IdType = none_t,
            typename R = typename request_type<T, true>::result_type,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename LookupKey,
            typename R = typename request_type<T, true>::result_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    using request = request_type<T>;
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = typename request_type<T, true>::result_type>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    using request = request_type<T>;
    using interface_type = typename request::interface_type;
    using value_type = typename request::value_type;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      const bool has_runtime_no_key =
          has_runtime_no_key_binding<interface_type, value_type>();
      if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<T>) {
        using no_key = detail::no_lookup_key_t;
        constexpr bool has_static_normalized_binding =
            is_static_binding_available_v<value_type, no_key>;

        if constexpr (has_static_construct_v<T>) {
          if (!has_runtime_no_key) {
            return execute_transaction(runtime_registry_.runtime(),
                                       [&](runtime_context_type &context) -> R {
                                         return construct_static<request, R>(
                                             ephemeral_scope, context);
                                       });
          }
        }

        if (has_runtime_no_key) {
          return execute_transaction(
              runtime_registry_.runtime(),
              [&](runtime_context_type &context) -> R {
                return construct_runtime_request<request, Factory, R>(
                    ephemeral_scope, context, std::move(factory));
              });
        }

        if constexpr (has_static_normalized_binding) {
          ::dingo::throw_missing_rvalue_conversion<T>(true);
        }

        return execute_transaction(
            runtime_registry_.runtime(),
            [&](runtime_context_type &context) -> R {
              return construct_runtime_request<request, Factory, R>(
                  ephemeral_scope, context, std::move(factory));
            });
      } else if constexpr (has_static_construct_v<T>) {
        if (!has_runtime_no_key) {
          return execute_transaction(runtime_registry_.runtime(),
                                     [&](runtime_context_type &context) -> R {
                                       return construct_static<request, R>(
                                           ephemeral_scope, context);
                                     });
        }
      } else {
        return execute_transaction(
            runtime_registry_.runtime(),
            [&](runtime_context_type &context) -> R {
              return construct_runtime_request<request, Factory, R>(
                  ephemeral_scope, context, std::move(factory));
            });
      }
    }
    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
          if constexpr (std::is_same_v<Factory,
                                       constructor<normalized_type_t<T>>>) {
            auto key = detail::no_lookup_key();
            const auto status = binding_status<T>(key);
            if (status != detail::binding_status::not_found) {
              if constexpr (construct_normalized_request_v<T>) {
                return ::dingo::construct_resolved_request<T>(
                    [&]() {
                      return resolve<request, typename request::lookup_type>(
                          ephemeral_scope, context, *this, key);
                    },
                    [&]() {
                      using normalized_request = request_type<value_type>;
                      return resolve<normalized_request,
                                     typename normalized_request::lookup_type>(
                          ephemeral_scope, context, *this, key);
                    });
              } else {
                return resolve<request, typename request::lookup_type>(
                    ephemeral_scope, context, *this, key);
              }
            } else if (binding_status<value_type>(key) !=
                       detail::binding_status::not_found) {
              if constexpr (construct_normalized_request_v<T>) {
                using normalized_request = request_type<value_type>;
                return type_traits<std::decay_t<T>>::make(
                    resolve<normalized_request,
                            typename normalized_request::lookup_type>(
                        ephemeral_scope, context, *this, key));
              } else {
                return resolve<request, typename request::lookup_type>(
                    ephemeral_scope, context, *this, key);
              }
            }
          }

          if constexpr (construct_factory_request_v<T>) {
            auto type_guard = context.template track_type<value_type>();
            return factory.template construct<R>(ephemeral_scope, context,
                                                 *this);
          } else {
            return resolve<request, typename request::lookup_type>(
                ephemeral_scope, context, *this, detail::no_lookup_key());
          }
        });
  }

  template <typename T> T construct_collection() {
    return construct_collection<T>(detail::no_lookup_key());
  }

  template <
      typename T, typename Fn,
      std::enable_if_t<!detail::is_lookup_key_v<std::decay_t<Fn>>, int> = 0>
  T construct_collection(Fn &&fn) {
    return construct_collection<T>(std::forward<Fn>(fn),
                                   detail::no_lookup_key());
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(LookupKey key) {
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      if (select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            runtime_registry_.runtime(),
            [&](runtime_context_type &context) -> T {
              return static_registry_
                  .template construct_collection<T, LookupKey>(ephemeral_scope,
                                                               *this, context);
            });
      }
    }

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> T {
          return construct_collection<T>(ephemeral_scope, context,
                                         detail::binding_collection_append{},
                                         std::move(key));
        });
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      if (select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            runtime_registry_.runtime(),
            [&](runtime_context_type &context) -> T {
              return static_registry_
                  .template construct_collection<T, LookupKey>(
                      ephemeral_scope, *this, context, std::forward<Fn>(fn));
            });
      }
    }

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> T {
          return construct_collection<T>(ephemeral_scope, context,
                                         std::forward<Fn>(fn), std::move(key));
        });
  }

  template <typename T, typename Key> T construct_collection(key_type<Key>) {
    return construct_collection<T>(detail::make_lookup_key(key_type<Key>{}));
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key_type<Key>) {
    return construct_collection<T>(std::forward<Fn>(fn),
                                   detail::make_lookup_key(key_type<Key>{}));
  }

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        detail::callable_dispatch_signature_t<Signature, callable_type>;

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) {
          auto type_guard = context.template track_type<callable_type>();
          return detail::callable_invoke<dispatch_signature>::construct(
              std::forward<Callable>(callable), ephemeral_scope, context,
              *this);
        });
  }

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(LookupKey key) {
    using request = request_type<Request>;
    using interface_type = typename request::interface_type;
    using static_selection = static_selection_t<interface_type, LookupKey>;
    const auto runtime_status =
        runtime_registry_.template binding_status<interface_type>(key);
    return detail::resolve_binding_status<static_selection::status>(
        runtime_status,
        detail::binding_resolution_policy::ambiguous_on_conflict);
  }

  template <typename T, typename Fn>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn) {
    return append_collection<T>(scope, results, context, std::forward<Fn>(fn),
                                detail::no_lookup_key());
  }

  template <typename T, typename Key, typename Fn>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn) {
    return append_collection<T>(scope, results, context, std::forward<Fn>(fn),
                                detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return append_collection_impl<T>(scope, results, context, std::move(key),
                                     std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection_impl(construction_scope scope, T &results,
                                     runtime_context_type &context,
                                     LookupKey key, Fn &&fn) {
    std::size_t count = 0;
    if constexpr (runtime_configuration::merge_parent_collections &&
                  has_parent_v) {
      if (parent_) {
        count += parent_->template append_collection<T>(results, scope, context,
                                                        fn, key);
      }
    }
    count += detail::append_binding_collection(
        results,
        [&](auto &collection, auto &&append) {
          return runtime_registry_.template append_collection<T>(
              collection, scope, context,
              std::forward<decltype(append)>(append), key);
        },
        [&](auto &collection, auto &&append) {
          return static_registry_.template append_collection<T, LookupKey>(
              scope, collection, *this, context,
              std::forward<decltype(append)>(append));
        },
        std::forward<Fn>(fn));
    return count;
  }

  template <typename T> std::size_t count_collection() {
    return count_collection<T>(detail::no_lookup_key());
  }

  template <typename T, typename Key> std::size_t count_collection() {
    return count_collection<T>(detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    std::size_t count = detail::count_binding_collection<T>(
        [&] { return runtime_registry_.template count_collection<T>(key); },
        static_registry_.template count_collection<T, LookupKey>());
    if constexpr (runtime_configuration::merge_parent_collections &&
                  has_parent_v) {
      if (parent_) {
        count += parent_->template count_collection<T>(key);
      }
    }
    return count;
  }

private:
  template <typename Request, typename R, typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(typename runtime_registry_type::runtime_selection selection,
            construction_scope scope, runtime_context_type &context,
            Origin &origin, LookupKey key) {
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename Request::lookup_type>(context);
    }
    if (selection.status == detail::binding_status::found) {
      return runtime_registry_
          .template resolve_binding<typename Request::interface_type, R>(
              selection, scope, context);
    }
    if constexpr (has_parent_v) {
      if (parent_) {
        return resolve_parent<Request>(scope, context, origin, key);
      }
    }
    return origin.template unresolved<
        Request, runtime_auto_constructible_v<typename Request::user_type>,
        LookupKey, R>(scope, context, key);
  }

  template <typename Request, typename R, typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_singular(construction_scope scope, runtime_context_type &context,
                     Origin &origin, LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    using result_type = R;
    auto runtime_selection =
        runtime_registry_.template select_binding<lookup_type>(key);
    using static_selection =
        typename static_registry_type::template selection<lookup_type,
                                                          LookupKey>;
    if (runtime_selection.status == detail::binding_status::ambiguous ||
        static_selection::status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<lookup_type>(context);
    }
    if constexpr (static_selection::status == detail::binding_status::found) {
      if (runtime_selection.status == detail::binding_status::found) {
        throw detail::make_type_ambiguous_exception<lookup_type>(context);
      }
      return static_registry_
          .template resolve_binding<lookup_type, result_type, static_selection>(
              scope, context, *this);
    } else {
      if (runtime_selection.status == detail::binding_status::found) {
        return runtime_registry_
            .template resolve_binding<result_type, result_type>(
                runtime_selection, scope, context);
      }
      if constexpr (has_parent_v) {
        if (parent_) {
          return resolve_parent<Request>(scope, context, origin, key);
        }
      }
      using user_type = typename Request::user_type;
      return origin.template unresolved<
          Request, runtime_auto_constructible_v<user_type>, LookupKey, R>(
          scope, context, key);
    }
  }

  template <typename Request, typename R, typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_collection(construction_scope scope, runtime_context_type &context,
                       Origin &origin, LookupKey key) {
    if constexpr (detail::is_static_lookup_key_definition_v<LookupKey>) {
      if (count_collection<R>(key) == 0 &&
          !runtime_registry_.template has_explicit_collection_lookup<R>(key)) {
        if constexpr (has_parent_v) {
          if (parent_) {
            return resolve_parent<Request>(scope, context, origin, key);
          }
        }
        return origin.template unresolved<Request, false, LookupKey, R>(
            scope, context, key);
      }
      return construct_collection<R>(
          scope, context, detail::binding_collection_append{}, std::move(key));
    } else {
      if constexpr (has_parent_v) {
        if (parent_ &&
            runtime_registry_.template count_collection<R>(key) == 0) {
          return resolve_parent<Request>(scope, context, origin, key);
        }
      }
      auto selection =
          runtime_registry_
              .template select_binding<typename Request::lookup_type>(key);
      if (selection.status == detail::binding_status::ambiguous) {
        throw detail::make_type_ambiguous_exception<
            typename Request::lookup_type>(context);
      }
      if (selection.status == detail::binding_status::found) {
        return runtime_registry_
            .template resolve_binding<typename Request::interface_type, R>(
                selection, scope, context);
      }
      return origin.template unresolved<Request, false, LookupKey, R>(
          scope, context, key);
    }
  }

  template <typename Request, typename R, typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            Origin &origin, LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      return resolve_collection<Request, R>(scope, context, origin,
                                            std::move(key));
    } else if constexpr (detail::is_static_lookup_key_definition_v<LookupKey>) {
      return resolve_singular<Request, R>(scope, context, origin,
                                          std::move(key));
    } else {
      auto selection =
          runtime_registry_
              .template select_binding<typename Request::lookup_type>(key);
      return resolve<Request, R>(selection, scope, context, origin,
                                 std::move(key));
    }
  }

  template <typename Request, typename R, typename Origin, typename LookupKey>
  DINGO_NOINLINE R resolve_nested(construction_scope scope,
                                  runtime_context_type &context, Origin &origin,
                                  LookupKey key) {
    return execute_transaction(runtime_registry_.runtime(), context,
                               [&](runtime_context_type &local_context) -> R {
                                 return resolve<Request, R>(
                                     scope, local_context, origin,
                                     std::move(key));
                               });
  }

public:
  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(construction_scope scope,
                                runtime_context_type &context, LookupKey key) {
    return resolve<T, RemoveRvalueReferences>(scope, context, *this,
                                              std::move(key));
  }

  template <typename T, bool RemoveRvalueReferences, typename Origin,
            typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(construction_scope scope,
                                runtime_context_type &context, Origin &origin,
                                LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    if (context.owns(runtime_registry_.runtime())) {
      return resolve<request, R>(scope, context, origin, std::move(key));
    }
    return resolve_nested<request, R>(scope, context, origin, std::move(key));
  }

  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            bool StaticError = false,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, runtime_context_type &context,
               LookupKey key) {
    (void)StaticError;
    using Type = typename Request::value_type;
    using user_type = typename Request::user_type;

    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (detail::default_collection_append_v<R>) {
        return construct_collection<R>(scope, context,
                                       detail::binding_collection_append{},
                                       std::move(key));
      } else {
        using resolve_type = typename collection_traits<R>::resolve_type;
        throw detail::make_collection_type_not_found_exception<R,
                                                               resolve_type>();
      }
    } else if constexpr (MayAutoConstruct &&
                         is_auto_constructible<
                             std::decay_t<user_type>>::value) {
      if constexpr (constructor<Type>::kind ==
                    detail::constructor_kind::concrete) {
        static_assert(is_complete<Type>::value,
                      "auto-construction requires a complete type");
        using type_detection = detail::constructor_shape;
        return context.template construct<typename Request::interface_type,
                                          type_detection>(scope, *this);
      } else {
        throw detail::make_type_not_found_exception<
            typename Request::lookup_type>(context, key);
      }
    } else {
      throw detail::make_type_not_found_exception<
          typename Request::lookup_type>(context, key);
    }
  }

private:
  friend class detail::runtime_registration_api<self_type>;

  runtime_registry_type &binding_store() { return runtime_registry_; }

  self_type &runtime_registration_parent() { return *this; }

  runtime_registry_type runtime_registry_;
  static_registry_type static_registry_;
  parent_container_type *parent_ = nullptr;
};

template <typename... Params>
class container : public detail::container_base_t<Params...> {
  using container_base_type = detail::container_base_t<Params...>;

public:
  using container_base_type::container_base_type;
};
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <dingo/static_container.h>
