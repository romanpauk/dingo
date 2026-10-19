//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/complete_type.h>
#include <dingo/base/config.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/collection.h>
#include <dingo/binding/model.h>
#include <dingo/binding/policy.h>
#include <dingo/binding/selection.h>
#include <dingo/container/base.h>
#include <dingo/container/collection.h>
#include <dingo/container/config.h>
#include <dingo/container/implementation.h>
#include <dingo/dependency/collection.h>
#include <dingo/dependency/normalized_type.h>
#include <dingo/dependency/request.h>
#include <dingo/dependency/selector.h>
#include <dingo/exposure/interface_storage_traits.h>
#include <dingo/factory/auto_constructible.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/invoke.h>
#include <dingo/key/annotated.h>
#include <dingo/lookup/definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/binding.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/registration.h>
#include <dingo/runtime/traits.h>
#include <dingo/static/activation/set.h>
#include <dingo/static/bindings.h>
#include <dingo/static/graph/graph.h>
#include <dingo/static/probe.h>
#include <dingo/static/registry.h>
#include <dingo/static/traits.h>

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

// One resolve front-end for all container shapes: runtime-only containers
// (container<>, runtime_container) are the form with empty static bindings
// and a runtime-only configuration, container<bindings<...>> adds the static
// registry, and registration-local containers use the merging configuration.
template <typename ParentContainer, typename RuntimeConfig,
          typename... Registrations>
class detail::container_with_static_bindings<static_bindings<Registrations...>,
                                             ParentContainer, RuntimeConfig>
    : public detail::runtime_registration_api<
          detail::container_with_static_bindings<
              static_bindings<Registrations...>, ParentContainer,
              RuntimeConfig>>,
      public detail::container_collection_api<
          detail::container_with_static_bindings<
              static_bindings<Registrations...>, ParentContainer,
              RuntimeConfig>,
          detail::static_container_runtime_context_t<
              detail::container_with_static_bindings<
                  static_bindings<Registrations...>, ParentContainer,
                  RuntimeConfig>,
              RuntimeConfig>>,
      // Empty bindings leave an empty base, so the object does not grow.
      private std::conditional_t<
          (sizeof...(Registrations) != 0),
          detail::static_registry<
              static_bindings<Registrations...>,
              detail::static_storage_state<Registrations...>>,
          none_t> {
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
  using collection_api_type =
      detail::container_collection_api<self_type, runtime_context_type>;
  friend collection_api_type;
  template <typename, typename, typename, typename, bool>
  friend class ::dingo::runtime_registry;

  static constexpr bool runtime_only_v = runtime_configuration::runtime_only;
  static constexpr bool has_static_v = sizeof...(Registrations) != 0;

public:
  // A runtime-only container without an explicit parent type accepts a parent
  // of its own type.
  using parent_container_type =
      std::conditional_t<std::is_void_v<ParentContainer> && runtime_only_v,
                         self_type,
                         detail::container_implementation_t<ParentContainer>>;
  using runtime_registry_type = runtime_base;
  using registry_type = runtime_base;
  using container_type = self_type;
  using container_traits_type = typename runtime_base::container_traits_type;
  using allocator_type = typename runtime_base::allocator_type;
  using rtti_type = typename runtime_base::rtti_type;
  using lookup_definition_type = typename runtime_base::lookup_definition_type;

  // The static registry base has the collection names as well.
  using collection_api_type::append_collection;
  using collection_api_type::construct_collection;
  using collection_api_type::count_collection;

private:
  static constexpr bool has_parent_v = !std::is_void_v<parent_container_type>;

  static_assert(
      detail::static_requirements<has_static_v, static_bindings_type,
                                  typename runtime_base::container_traits_type,
                                  RuntimeConfig>::value);

  template <typename T, typename Key>
  using static_selection_t =
      typename static_bindings_type::template selection<T, Key>;

  template <typename T>
  static constexpr bool runtime_auto_constructible_v =
      detail::is_runtime_auto_constructible_dependency_v<T>;

  template <typename Lookup, typename Key>
  static constexpr detail::binding_status static_status_v = [] {
    if constexpr (has_static_v) {
      return static_selection_t<Lookup, Key>::status;
    } else {
      return detail::binding_status::not_found;
    }
  }();

  // Runtime-only containers fall back to the parent only for lookups the
  // parent declares; containers with static bindings always do.
  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves() {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    if constexpr (runtime_only_v) {
      return has_parent_v &&
             detail::has_lookup<parent_container_type, Lookup, LookupKey>();
    } else {
      return has_parent_v;
    }
  }

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_binding(LookupKey key) {
    return runtime_registry_.template binding_status<
               typename request_type<Request>::value_type,
               typename request_type<Request>::exact_type>(key) !=
           detail::binding_status::not_found;
  }

  template <typename Request, typename NormalizedRequest>
  bool has_runtime_no_key_binding() {
    auto key = detail::no_lookup_key();
    static_assert(std::is_same_v<request_value_t<Request>,
                                 request_value_t<NormalizedRequest>>);
    return has_runtime_binding<Request>(key);
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_collection(LookupKey key) {
    return runtime_registry_.template count_collection<T>(key) != 0;
  }

  template <typename Request, typename Key,
            bool Found =
                static_status_v<Request, Key> == detail::binding_status::found>
  struct is_static_binding_available : std::false_type {};

  template <typename Request, typename Key>
  struct is_static_binding_available<Request, Key, true> {
    using binding = typename static_selection_t<Request, Key>::binding_type;
    static constexpr bool value =
        detail::binding_supports_request_v<request_result_t<Request>, binding>;
  };

  template <typename Request, typename Key>
  static constexpr bool is_static_binding_available_v =
      is_static_binding_available<Request, Key>::value;
  template <typename T>
  static constexpr bool has_static_construct_v = [] {
    if constexpr (has_static_v) {
      using request = request_type<T>;
      using interface_type = typename request::interface_type;
      using value_type = typename request::value_type;
      using no_key = detail::no_lookup_key_t;
      constexpr bool has_exact_static_binding =
          is_static_binding_available_v<interface_type, no_key>;
      constexpr bool has_normalized_static_binding =
          is_static_binding_available_v<value_type, no_key>;

      return has_exact_static_binding || (has_normalized_static_binding &&
                                          construct_normalized_request_v<T>);
    } else {
      return false;
    }
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

  static_registry_type &statics() { return *this; }

  // The runtime registry access of container_collection_api.
  auto &runtime() { return runtime_registry_.runtime(); }

  template <typename T, typename LookupKey>
  std::size_t runtime_count_collection(LookupKey key) {
    return runtime_registry_.template count_collection<T>(key);
  }

  template <typename T, typename LookupKey>
  bool runtime_has_explicit_collection_lookup(const LookupKey &key) {
    return runtime_registry_.template has_explicit_collection_lookup<T>(key);
  }

  template <typename T, typename... Args>
  std::size_t runtime_append_collection(Args &&...args) {
    return runtime_registry_.template append_collection<T>(
        std::forward<Args>(args)...);
  }

  template <typename Request, typename LookupKey, typename Context,
            typename R = typename Request::result_type>
  DINGO_ALWAYS_INLINE R resolve_static(construction_scope scope,
                                       Context &context) {
    return statics().template resolve_request<Request, LookupKey>(
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
    auto selected = runtime_registry_.template select_binding<
        typename Request::value_type, typename Request::exact_type>(key);
    if (selected.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename Request::lookup_type>(context);
    }
    if (selected.status == detail::binding_status::found) {
      return runtime_registry_
          .template resolve_binding<typename Request::interface_type, R>(
              selected, scope, context);
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
      if (runtime_registry_.template binding_status<
              typename Request::value_type, typename Request::exact_type>(
              key) != detail::binding_status::not_found) {
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

  // Runtime-only containers resolve through their parent when constructing.
  template <typename Request, typename R = typename Request::result_type>
  R construct_resolved_request(construction_scope scope,
                               runtime_context_type &context) {
    using user_type = typename Request::user_type;
    try {
      return resolve<Request, R, runtime_auto_constructible_v<user_type>>(
          scope, context, *this, detail::no_lookup_key());
    } catch (const type_not_convertible_exception &) {
      using request_value_type = typename Request::value_type;
      using normalized = request_type<request_value_type>;
      auto &&value =
          resolve<normalized, typename normalized::lookup_type, true>(
              scope, context, *this, detail::no_lookup_key());
      return type_traits<std::decay_t<user_type>>::make(
          std::forward<decltype(value)>(value));
    }
  }

  template <typename Request,
            typename Factory = constructor<typename Request::value_type>,
            typename R = typename Request::result_type>
  R construct_request(construction_scope scope, runtime_context_type &context,
                      Factory factory = Factory()) {
    using user_type = typename Request::user_type;
    using request_value_type = typename Request::value_type;
    constexpr bool may_auto = runtime_auto_constructible_v<user_type>;

    if constexpr (std::is_same_v<Factory, constructor<request_value_type>>) {
      auto key = detail::no_lookup_key();
      if (binding_status<typename Request::lookup_type>(key) !=
          detail::binding_status::not_found) {
        if constexpr (!construct_normalized_request_v<user_type> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          user_type>) {
          return resolve<Request, R, may_auto>(scope, context, *this, key);
        } else {
          return construct_resolved_request<Request, R>(scope, context);
        }
      }
    }

    if constexpr (construct_factory_request_v<user_type>) {
      return factory.template construct<R>(scope, context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             user_type>) {
      ::dingo::throw_missing_rvalue_conversion<user_type>(false, context);
    } else {
      auto key = detail::no_lookup_key();
      return resolve<Request, R, may_auto>(scope, context, *this, key);
    }
  }

public:
  template <bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<OwnsRuntimeData, int> = 0>
  container_with_static_bindings()
      : runtime_registry_(detail::runtime_data_owner, this) {}

  template <bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<OwnsRuntimeData, int> = 0>
  explicit container_with_static_bindings(allocator_type alloc)
      : runtime_registry_(detail::runtime_data_owner, this, alloc) {}

  template <bool RuntimeOnly = runtime_only_v,
            std::enable_if_t<RuntimeOnly, int> = 0>
  container_with_static_bindings(parent_container_type *parent,
                                 const allocator_type &alloc = allocator_type())
      : runtime_registry_(detail::runtime_data_owner, this, alloc),
        parent_(parent) {}

  template <typename Parent = ParentContainer,
            bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
            std::enable_if_t<!std::is_void_v<Parent> && OwnsRuntimeData &&
                                 !runtime_only_v,
                             int> = 0>
  explicit container_with_static_bindings(
      Parent *parent, allocator_type alloc = allocator_type())
      : runtime_registry_(detail::runtime_data_owner, this, alloc),
        parent_(detail::container_implementation_of(parent)) {}

  template <
      typename Parent = ParentContainer,
      bool OwnsRuntimeData = runtime_configuration::owns_runtime_data,
      std::enable_if_t<!std::is_void_v<Parent> && !OwnsRuntimeData, int> = 0>
  explicit container_with_static_bindings(
      Parent *parent, allocator_type alloc = allocator_type())
      : runtime_registry_(parent, alloc), parent_(parent) {}

  // Compile-time bindings keep their instances inline, resolved instances
  // reference each other by address, and runtime bindings point back at the
  // container, so it cannot move. runtime_container moves by owning it through
  // a handle.
  container_with_static_bindings(const self_type &) = delete;
  container_with_static_bindings(self_type &&) = delete;
  self_type &operator=(const self_type &) = delete;
  self_type &operator=(self_type &&) = delete;

  self_type &container() { return *this; }

  const self_type &container() const { return *this; }

private:
  template <typename Request, typename R, typename LookupKey>
  static constexpr bool is_cacheable() {
    if constexpr (!detail::cache::supports_v<
                      typename Request::interface_type> ||
                  collection_traits<R>::is_collection) {
      return false;
    } else if constexpr (has_static_v &&
                         detail::is_static_lookup_key_definition_v<LookupKey>) {
      return static_status_v<typename Request::lookup_type, LookupKey> ==
             detail::binding_status::not_found;
    } else {
      return true;
    }
  }

  template <typename Request, typename R>
  DINGO_NOINLINE R
  resolve_selected(typename runtime_registry_type::runtime_selection selected) {
    using interface_type = typename Request::interface_type;
    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
          return runtime_registry_.template resolve_binding<interface_type, R>(
              selected, ephemeral_scope, context);
        });
  }

  template <typename Request, typename R, typename LookupKey>
  R resolve_entry(LookupKey key) {
    using interface_type = typename Request::interface_type;
    if constexpr (is_cacheable<Request, R, LookupKey>()) {
      auto selected = runtime_registry_.template select_binding<
          typename Request::value_type, typename Request::exact_type>(key);
      auto result =
          runtime_registry_.template lookup_cache<interface_type>(selected);
      if (result.hit) {
        return runtime_registry_.template resolve_cached<interface_type, R>(
            result);
      }
      if (selected.status == detail::binding_status::found) {
        return resolve_selected<Request, R>(selected);
      }
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            // MSVC's pre-C++20 lambda processor does not treat an enclosing
            // constexpr local as a constant expression, so spell it here.
            return resolve<
                Request, R,
                runtime_auto_constructible_v<typename Request::user_type>>(
                selected, ephemeral_scope, context, *this, std::move(key));
          });
    } else {
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            return resolve<
                Request, R,
                runtime_auto_constructible_v<typename Request::user_type>>(
                ephemeral_scope, context, *this, std::move(key));
          });
    }
  }

public:
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T, true>,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    using request = request_type<T>;
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T, true>>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    using request = request_type<T>;
    using interface_type = typename request::interface_type;
    using value_type = typename request::value_type;
    if constexpr (runtime_only_v) {
      return execute_transaction(
          runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
            return construct_request<request, Factory, R>(
                ephemeral_scope, context, std::move(factory));
          });
    } else {
      if constexpr (std::is_same_v<Factory,
                                   constructor<normalized_type_t<T>>>) {
        const bool has_runtime_no_key =
            has_runtime_no_key_binding<interface_type, value_type>();
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          using no_key = detail::no_lookup_key_t;
          constexpr bool has_static_normalized_binding =
              is_static_binding_available_v<value_type, no_key>;

          if constexpr (has_static_construct_v<T>) {
            if (!has_runtime_no_key) {
              return execute_transaction(
                  runtime_registry_.runtime(),
                  [&](runtime_context_type &context) -> R {
                    return construct_static<request, R>(ephemeral_scope,
                                                        context);
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
                        return resolve<
                            normalized_request,
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

  template <typename Request, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    using request = request_type<Request>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    using lookup_key_type = decltype(key);
    if constexpr (runtime_only_v) {
      auto status = runtime_registry_.template binding_status<
          typename request::value_type, typename request::exact_type>(key);
      if constexpr (parent_serves<typename request::lookup_type,
                                  lookup_key_type>()) {
        if (parent_ && status == detail::binding_status::not_found) {
          return parent_->template binding_status<Request>(key);
        }
      }
      return status;
    } else {
      using interface_type = typename request::interface_type;
      const auto runtime_status = runtime_registry_.template binding_status<
          typename request_type<interface_type>::value_type,
          typename request_type<interface_type>::exact_type>(key);
      return detail::resolve_binding_status<
          static_status_v<interface_type, lookup_key_type>>(
          runtime_status,
          detail::binding_resolution_policy::ambiguous_on_conflict);
    }
  }

private:
  template <typename Request, typename R, bool MayAutoConstruct,
            typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(typename runtime_registry_type::runtime_selection selected,
            construction_scope scope, runtime_context_type &context,
            Origin &origin, LookupKey key) {
    if (selected.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename Request::lookup_type>(context);
    }
    if (selected.status == detail::binding_status::found) {
      return runtime_registry_
          .template resolve_binding<typename Request::interface_type, R>(
              selected, scope, context);
    }
    if constexpr (parent_serves<typename Request::lookup_type, LookupKey>()) {
      if (parent_) {
        return resolve_parent<Request>(scope, context, origin, key);
      }
    }
    return origin.template unresolved<Request, MayAutoConstruct, LookupKey, R>(
        scope, context, key);
  }

  template <typename Request, typename R, bool MayAutoConstruct,
            typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_singular(construction_scope scope, runtime_context_type &context,
                     Origin &origin, LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    using result_type = R;
    constexpr auto static_status = static_status_v<lookup_type, LookupKey>;
    auto runtime_selection = runtime_registry_.template select_binding<
        typename Request::value_type, typename Request::exact_type>(key);
    if (runtime_selection.status == detail::binding_status::ambiguous ||
        static_status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<lookup_type>(context);
    }
    if constexpr (static_status == detail::binding_status::found) {
      if (runtime_selection.status == detail::binding_status::found) {
        throw detail::make_type_ambiguous_exception<lookup_type>(context);
      }
      return statics()
          .template resolve_binding<lookup_type, result_type,
                                    static_selection_t<lookup_type, LookupKey>>(
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
      return origin
          .template unresolved<Request, MayAutoConstruct, LookupKey, R>(
              scope, context, key);
    }
  }

  template <typename Request, typename R, bool MayAutoConstruct,
            typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  R resolve_collection(construction_scope scope, runtime_context_type &context,
                       Origin &origin, LookupKey key) {
    if constexpr (runtime_only_v) {
      if constexpr (detail::is_no_lookup_key_v<LookupKey>) {
        auto selected = runtime_registry_.template select_binding<
            typename Request::value_type, typename Request::exact_type>(key);
        if (selected.status == detail::binding_status::ambiguous) {
          throw detail::make_type_ambiguous_exception<
              typename Request::lookup_type>(context);
        }
        if (selected.status == detail::binding_status::found) {
          return runtime_registry_
              .template resolve_binding<typename Request::interface_type, R>(
                  selected, scope, context);
        }
      }
      if (this->template count_collection<R>(key) == 0) {
        if constexpr (parent_serves<typename collection_traits<R>::resolve_type,
                                    LookupKey>()) {
          if (parent_) {
            return resolve_parent<Request>(scope, context, origin, key);
          }
        }
        return origin.template unresolved<Request, false, LookupKey, R>(
            scope, context, key);
      }
      // Collections are constructed in the active resolve context.  Routing
      // no-key collections through scalar registry resolution would bypass
      // registered collection factories and re-enter missing-binding fallback.
      if constexpr (detail::default_collection_append_v<R>) {
        return this->template construct_collection_scoped<R>(
            scope, context, detail::binding_collection_append{},
            std::move(key));
      } else {
        return origin.template unresolved<Request, false, LookupKey, R>(
            scope, context, key);
      }
    } else if constexpr (detail::is_static_lookup_key_definition_v<LookupKey>) {
      if (this->template count_collection<R>(key) == 0 &&
          !runtime_registry_.template has_explicit_collection_lookup<R>(key)) {
        if constexpr (has_parent_v) {
          if (parent_) {
            return resolve_parent<Request>(scope, context, origin, key);
          }
        }
        return origin.template unresolved<Request, false, LookupKey, R>(
            scope, context, key);
      }
      return this->template construct_collection_scoped<R>(
          scope, context, detail::binding_collection_append{}, std::move(key));
    } else {
      if constexpr (has_parent_v) {
        if (parent_ &&
            runtime_registry_.template count_collection<R>(key) == 0) {
          return resolve_parent<Request>(scope, context, origin, key);
        }
      }
      auto selected = runtime_registry_.template select_binding<
          typename Request::value_type, typename Request::exact_type>(key);
      if (selected.status == detail::binding_status::ambiguous) {
        throw detail::make_type_ambiguous_exception<
            typename Request::lookup_type>(context);
      }
      if (selected.status == detail::binding_status::found) {
        return runtime_registry_
            .template resolve_binding<typename Request::interface_type, R>(
                selected, scope, context);
      }
      return origin.template unresolved<Request, false, LookupKey, R>(
          scope, context, key);
    }
  }

  template <typename Request, typename R,
            bool MayAutoConstruct =
                runtime_auto_constructible_v<typename Request::user_type>,
            typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, runtime_context_type &context,
            Origin &origin, LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      return resolve_collection<Request, R, MayAutoConstruct>(
          scope, context, origin, std::move(key));
    } else if constexpr (!runtime_only_v &&
                         detail::is_static_lookup_key_definition_v<LookupKey>) {
      return resolve_singular<Request, R, MayAutoConstruct>(
          scope, context, origin, std::move(key));
    } else {
      auto selected = runtime_registry_.template select_binding<
          typename Request::value_type, typename Request::exact_type>(key);
      return resolve<Request, R, MayAutoConstruct>(selected, scope, context,
                                                   origin, std::move(key));
    }
  }

  template <typename Request, typename R, bool MayAutoConstruct,
            typename Origin, typename LookupKey>
  DINGO_NOINLINE R resolve_nested(construction_scope scope,
                                  runtime_context_type &context, Origin &origin,
                                  LookupKey key) {
    return execute_transaction(runtime_registry_.runtime(), context,
                               [&](runtime_context_type &local_context) -> R {
                                 return resolve<Request, R, MayAutoConstruct>(
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
    constexpr bool may_auto = runtime_auto_constructible_v<T>;
    if (context.owns(runtime_registry_.runtime())) {
      return resolve<request, R, may_auto>(scope, context, origin,
                                           std::move(key));
    }
    return resolve_nested<request, R, may_auto>(scope, context, origin,
                                                std::move(key));
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
        return this->template construct_collection_scoped<R>(
            scope, context, detail::binding_collection_append{},
            std::move(key));
      } else if constexpr (runtime_only_v) {
        throw detail::make_type_not_found_exception<
            typename Request::lookup_type>(context, key);
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
