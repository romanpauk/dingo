//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The public operations of a container, independent of how its sources are
// configured. The container provides the resolve entry; this layer owns the
// spellings, the key normalization, and the static minimal-context decision.

#include <dingo/base/config.h>
#include <dingo/base/construction_scope.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/invoke.h>
#include <dingo/runtime/context.h>
#include <dingo/static/construction.h>
#include <dingo/static/resolution_policy.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Self, bool RuntimeSource> class container_operations {
public:
  // A runtime-backed container resolves an id by building its lookup key.
  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T, true>,
            std::enable_if_t<RuntimeSource && !detail::is_lookup_key_v<IdType>,
                             int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return self().template resolve_entry<request, R>(std::move(key));
  }

  // A static-only container spells the compile-time key as a template
  // argument. The keyed overload below performs the resolution.
  template <typename T, typename Key = key_type<none_t>,
            typename R = request_result_t<T, true>,
            std::enable_if_t<!RuntimeSource && !std::is_void_v<T>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve() {
    using lookup_key_type =
        decltype(detail::make_lookup_key(type_selector<Key>{}));
    return resolve<T, lookup_key_type, R>(
        detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    if constexpr (RuntimeSource) {
      using request = request_type<T>;
      return self().template resolve_entry<request, R>(std::move(key));
    } else {
      using lookup_request = request_type<T, true>;
      using request = request_type<T, false>;
      using check =
          typename Self::template resolve_request_check<request, R, LookupKey>;
      (void)sizeof(check);
      using policy =
          resolution_policy<T, LookupKey, typename Self::static_bindings_type,
                            resolution_mode::compile_time>;
      return policy::template resolve<R>(
          [&](auto) -> R {
            if constexpr (!collection_traits<R>::is_collection) {
              if constexpr (Self::template uses_static_minimal_context_v<
                                request, R, LookupKey>) {
                using selected =
                    typename Self::static_registry_type::template selection<
                        typename lookup_request::lookup_type, LookupKey>;
                typename Self::template static_resolution_context_t<request, R,
                                                                    LookupKey>
                    context;
                return self()
                    .static_registry_.template resolve_binding<
                        typename lookup_request::lookup_type, R, selected>(
                        ephemeral_scope, context, self());
              }
            }
            typename Self::context_type context;
            return self().selection_type::template resolve<request, R>(
                ephemeral_scope, context, self(), key);
          },
          [&](auto tag) -> R {
            using selected_policy = typename decltype(tag)::policy;
            return selected_policy::lift_type::make(
                resolve<typename selected_policy::inner_type, LookupKey,
                        request_result_t<typename selected_policy::inner_type,
                                         true>>(key));
          });
    }
  }

  // The dependency-path entry: a nested request resolves through the selected
  // source. A runtime-backed container decides whether the current context
  // already owns the runtime; a static-only container keeps its direct stored
  // read fast path.
  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename Origin, typename LookupKey,
            typename R = request_result_t<T, RemoveRvalueReferences>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(construction_scope scope, Context &context,
                                Origin &origin, LookupKey key) {
    if constexpr (RuntimeSource) {
      using request = request_type<T, RemoveRvalueReferences>;
      using policy =
          resolution_policy<T, LookupKey, typename Self::static_bindings_type,
                            resolution_mode::runtime_fallback>;
      if constexpr (!policy::lift_enabled) {
        if (self().owns_runtime(context)) {
          return self().selection_type::template resolve<request, R>(
              scope, context, origin, std::move(key));
        }
        return self().template resolve_nested<request, R>(
            scope, context, origin, std::move(key));
      }
      return policy::template resolve<R>(
          [&](auto) -> R {
            if (self().owns_runtime(context)) {
              return self().selection_type::template resolve<request, R>(
                  scope, context, origin, std::move(key));
            }
            return self().template resolve_nested<request, R>(
                scope, context, origin, std::move(key));
          },
          [&](auto tag) -> R {
            using selected_policy = typename decltype(tag)::policy;
            return selected_policy::lift_type::make(
                resolve<typename selected_policy::inner_type, false>(
                    scope, context, origin, key));
          });
    } else {
      using lookup_request = request_type<T, true>;
      using request = request_type<T, RemoveRvalueReferences>;
      using policy =
          resolution_policy<T, LookupKey, typename Self::static_bindings_type,
                            resolution_mode::compile_time>;
      return policy::template resolve<R>(
          [&](auto) -> R {
            using lookup_selection =
                typename Self::static_bindings_type::template selection<
                    typename lookup_request::lookup_type, LookupKey>;
            constexpr bool fast_path =
                !collection_traits<R>::is_collection &&
                lookup_selection::status == binding_status::found;
            if constexpr (fast_path) {
              using binding = typename lookup_selection::binding_type;
              using binding_model_type = typename binding::binding_model_type;
              if constexpr (binding_supports_request_v<R, binding> &&
                            (std::is_reference_v<R> || std::is_pointer_v<R>) &&
                            stored_request_identity_v<
                                R, typename binding_model_type::storage_type> &&
                            factory_without_dependencies_v<
                                typename binding_model_type::factory_type>) {
                using selected =
                    typename Self::static_registry_type::template selection<
                        typename lookup_request::lookup_type, LookupKey>;
                return self()
                    .static_registry_.template resolve_binding<
                        typename lookup_request::lookup_type, R, selected>(
                        scope, context, self());
              }
            }
            return self().selection_type::template resolve<request, R>(
                scope, context, origin, key);
          },
          [&](auto tag) -> R {
            using selected_policy = typename decltype(tag)::policy;
            return selected_policy::lift_type::make(
                resolve<typename selected_policy::inner_type, false>(
                    scope, context, origin, key));
          });
    }
  }

  // Construction: the runtime tree decides between the runtime-only path, the
  // static-only fast path, and the mixed fallback; the static-only path is the
  // exact/normalized/factory rule over the compile-time source.
  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T, true>>
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    if constexpr (RuntimeSource) {
      using request = request_type<T>;
      using interface_type = typename request::interface_type;
      using value_type = typename request::value_type;
      if constexpr (Self::runtime_only_v) {
        return execute_transaction(
            self().runtime_storage(), [&](auto &context) -> R {
              return self().template construct_request<request, Factory, R>(
                  ephemeral_scope, context, std::move(factory));
            });
      } else {
        constexpr bool default_factory =
            std::is_same_v<Factory, constructor<normalized_type_t<T>>>;
        constexpr bool rvalue =
            ::dingo::rvalue_request_requires_explicit_conversion_v<T>;
        if constexpr (default_factory && rvalue) {
          return self()
              .template construct_rvalue_request<T, request, Factory, R>(
                  factory);
        }
        if constexpr (default_factory && !rvalue &&
                      !Self::template has_static_construct_v<T>) {
          return self()
              .template construct_runtime_transaction<request, Factory, R>(
                  factory);
        }
        if constexpr (default_factory && !rvalue &&
                      Self::template has_static_construct_v<T>) {
          if (!self()
                   .template has_runtime_no_key_binding<interface_type,
                                                        value_type>()) {
            return self().template construct_static_transaction<request, R>();
          }
        }

        return execute_transaction(
            self().runtime_storage(), [&](auto &context) -> R {
              return self()
                  .template construct_mixed_request<T, request, Factory, R>(
                      ephemeral_scope, context, factory);
            });
      }
    } else {
      using request = request_type<T>;
      using user_type = typename request::user_type;
      using interface_type = typename request::interface_type;
      using request_value_type = typename request::value_type;
      if constexpr (std::is_same_v<Factory, constructor<request_value_type>>) {
        using exact_selection =
            typename Self::static_bindings_type::template selection<
                binding_dependency_interface_t<interface_type>,
                binding_dependency_key_t<interface_type>>;
        using normalized_selection =
            typename Self::static_bindings_type::template selection<
                request_value_type, detail::no_lookup_key_t>;
        typename Self::context_type context;
        return detail::construct_from_static_source<
            request, R, typename Self::context_type, true, exact_selection,
            normalized_selection>(
            ephemeral_scope, context,
            [&](auto) { return self().template resolve<user_type>(); },
            [&](auto) { return self().template resolve<request_value_type>(); },
            [&](auto) {
              auto type_guard =
                  context.template track_type<request_value_type>();
              return factory.template construct<R>(ephemeral_scope, context,
                                                   self());
            });
      } else {
        typename Self::context_type context;
        auto type_guard = context.template track_type<request_value_type>();
        return factory.template construct<R>(ephemeral_scope, context, self());
      }
    }
  }

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    if constexpr (RuntimeSource) {
      return execute_transaction(self().runtime_storage(), [&](auto &context) {
        return detail::container_invoke<Signature>(
            context, self(), ephemeral_scope, std::forward<Callable>(callable));
      });
    } else {
      typename Self::context_type context;
      return detail::container_invoke<Signature>(
          context, self(), ephemeral_scope, std::forward<Callable>(callable));
    }
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn, LookupKey key) {
    if constexpr (RuntimeSource) {
      std::size_t count = 0;
      if constexpr (Self::merge_parent_collections && Self::has_parent) {
        if (self().parent_) {
          count += self().parent_->template append_collection<T>(
              scope, results, context, fn, key);
        }
      }
      if constexpr (Self::has_static_source) {
        count += detail::append_binding_collection(
            results,
            [&](auto &collection, auto &&append) {
              return self().template runtime_collect<T>(
                  scope, collection, context,
                  std::forward<decltype(append)>(append), key);
            },
            [&](auto &collection, auto &&append) {
              return self().statics().template collect<T, LookupKey>(
                  scope, collection, self(), context,
                  std::forward<decltype(append)>(append));
            },
            std::forward<Fn>(fn));
      } else {
        count += self().template runtime_collect<T>(scope, results, context,
                                                    std::forward<Fn>(fn), key);
      }
      return count;
    } else {
      (void)key;
      return self().static_registry_.template collect<T, LookupKey>(
          scope, results, self(), context, std::forward<Fn>(fn));
    }
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    if constexpr (RuntimeSource) {
      std::size_t count = self().template runtime_count<T>(key);
      if constexpr (Self::has_static_source) {
        count +=
            Self::static_registry_type::template fixed_count<T, LookupKey>();
      }
      if constexpr (Self::merge_parent_collections && Self::has_parent) {
        if (self().parent_) {
          count += self().parent_->template count_collection<T>(key);
        }
      }
      return count;
    } else {
      (void)key;
      return Self::static_registry_type::template fixed_count<T, LookupKey>();
    }
  }

  template <typename T, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope, Context &context,
                         LookupKey key) {
    if constexpr (RuntimeSource) {
      return construct_collection<T>(
          scope, context, detail::binding_collection_append{}, std::move(key));
    } else {
      (void)key;
      return self().template construct_collection_static<T, LookupKey>(scope,
                                                                       context);
    }
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope, Context &context, Fn &&fn,
                         LookupKey key) {
    if constexpr (RuntimeSource) {
      return detail::construct_binding_collection<T>(
          self().template count_collection<T>(key),
          self().template runtime_has_explicit_collection_lookup<T>(key),
          [&](T &results) {
            self().template append_collection<T>(scope, results, context, fn,
                                                 key);
          });
    } else {
      (void)key;
      return self().template construct_collection_static<T, LookupKey>(
          scope, context, std::forward<Fn>(fn));
    }
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_by_key(LookupKey key) {
    if constexpr (RuntimeSource) {
      if constexpr (Self::has_static_source &&
                    detail::is_static_lookup_key_v<LookupKey>) {
        if (self()
                .template select_static_collection_construct<T, LookupKey>()) {
          return execute_transaction(
              self().runtime_storage(), [&](auto &context) -> T {
                return self()
                    .statics()
                    .template construct_collection<T, LookupKey>(
                        ephemeral_scope, self(), context);
              });
        }
      }
      return execute_transaction(
          self().runtime_storage(), [&](auto &context) -> T {
            return construct_collection<T>(ephemeral_scope, context,
                                           detail::binding_collection_append{},
                                           std::move(key));
          });
    } else {
      typename Self::context_type context;
      return construct_collection<T>(ephemeral_scope, context, std::move(key));
    }
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_by_key(Fn &&fn, LookupKey key) {
    if constexpr (RuntimeSource) {
      if constexpr (Self::has_static_source &&
                    detail::is_static_lookup_key_v<LookupKey>) {
        if (self()
                .template select_static_collection_construct<T, LookupKey>()) {
          return execute_transaction(
              self().runtime_storage(), [&](auto &context) -> T {
                return self()
                    .statics()
                    .template construct_collection<T, LookupKey>(
                        ephemeral_scope, self(), context, std::forward<Fn>(fn));
              });
        }
      }
      return execute_transaction(
          self().runtime_storage(), [&](auto &context) -> T {
            return construct_collection<T>(
                ephemeral_scope, context, std::forward<Fn>(fn), std::move(key));
          });
    } else {
      typename Self::context_type context;
      return construct_collection<T>(ephemeral_scope, context,
                                     std::forward<Fn>(fn), std::move(key));
    }
  }

private:
  Self &self() { return static_cast<Self &>(*this); }
};

} // namespace detail
} // namespace dingo
