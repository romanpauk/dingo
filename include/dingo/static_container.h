//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/container_miss.h>
#include <dingo/container.h>
#include <dingo/container_api.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/static/activation_set.h>
#include <dingo/static/container_traits.h>
#include <dingo/static/context.h>
#include <dingo/static/resolution.h>
#include <dingo/static/resolution_policy.h>
#include <dingo/type/dependency_traits.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

namespace detail {

struct static_container_no_dependency_diagnostics {};

template <typename... Bindings> struct static_bindings_cat;

template <> struct static_bindings_cat<> {
  using type = static_bindings<>;
};

template <typename... Registrations>
struct static_bindings_cat<static_bindings<Registrations...>> {
  using type = static_bindings<Registrations...>;
};

template <typename... FirstRegistrations, typename... SecondRegistrations,
          typename... Tail>
struct static_bindings_cat<static_bindings<FirstRegistrations...>,
                           static_bindings<SecondRegistrations...>, Tail...>
    : static_bindings_cat<
          static_bindings<FirstRegistrations..., SecondRegistrations...>,
          Tail...> {};

template <typename... Bindings>
using static_bindings_cat_t = typename static_bindings_cat<Bindings...>::type;

template <typename Container, typename = void>
struct static_container_context_bindings {
  using type = static_bindings<>;
};

template <typename Container>
struct static_container_context_bindings<
    Container, std::void_t<typename Container::context_static_bindings_type>> {
  using type = typename Container::context_static_bindings_type;
};

template <typename Container>
using static_container_context_bindings_t =
    typename static_container_context_bindings<Container>::type;

template <typename StaticRegistry, bool HasParent>
struct static_container_dependency_diagnostics_base;

template <typename... Registrations>
struct static_container_dependency_diagnostics_base<
    static_bindings<Registrations...>, false>
    : static_registry_dependency_diagnostics<
          typename static_bindings<Registrations...>::interface_bindings,
          binding_model<Registrations>...> {};

template <typename... Registrations>
struct static_container_dependency_diagnostics_base<
    static_bindings<Registrations...>, true>
    : static_container_no_dependency_diagnostics {};

template <typename StaticRegistry, typename ParentContainer = void,
          typename ContainerTraits = ::dingo::static_container_traits>
class static_container_impl;

template <typename ParentContainer, typename ContainerTraits,
          typename... Registrations>
class static_container_impl<static_bindings<Registrations...>, ParentContainer,
                            ContainerTraits>
    : private static_container_dependency_diagnostics_base<
          static_bindings<Registrations...>, !std::is_void_v<ParentContainer>>,
      public container_api<
          static_container_impl<static_bindings<Registrations...>,
                                ParentContainer, ContainerTraits>> {
public:
  using static_bindings_type = static_bindings<Registrations...>;
  using context_static_bindings_type = static_bindings_cat_t<
      static_bindings_type,
      static_container_context_bindings_t<ParentContainer>>;

private:
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using graph_type_ = graph_analysis<static_bindings_type, has_parent_v>;
  using parent_container_type = container_implementation_t<ParentContainer>;
  using state_type = static_storage_state<Registrations...>;
  using context_type = static_context<context_static_bindings_type>;
  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<ContainerTraits>>;

  template <typename Policy, bool MayAutoConstruct, bool StaticError,
            typename Request, typename LookupKey, typename R,
            typename FriendContainer, typename FriendContext>
  friend R resolve_missing_request(FriendContainer &, construction_scope,
                                   FriendContext &, LookupKey);

  template <typename Request, typename Key>
  using selection_t =
      typename static_bindings_type::template selection<Request, Key>;

  template <typename Request, typename Key>
  static constexpr binding_status resolve_status_v =
      selection_t<typename Request::lookup_type, Key>::status;

  template <typename Request, typename Result, typename Selection,
            binding_status Status = Selection::status>
  struct minimal_context_selection {
    static constexpr bool value = false;
  };

  template <typename Request, typename Result, typename Selection>
  struct minimal_context_selection<Request, Result, Selection,
                                   binding_status::found> {
    using binding = typename Selection::binding_type;
    using binding_model_type = typename binding::binding_model_type;

    static constexpr bool value =
        (std::is_reference_v<Result> || std::is_pointer_v<Result>) &&
        binding_supports_request_v<typename Request::interface_type, binding> &&
        stored_request_identity_v<typename Request::interface_type,
                                  typename binding_model_type::storage_type> &&
        factory_without_dependencies_v<
            typename binding_model_type::factory_type>;
  };

  template <typename Request, typename LookupKey,
            binding_status Status =
                resolve_status_v<Request, detail::make_lookup_key_t<LookupKey>>>
  struct resolve_request_status_check {
    using type = void;
  };

  template <typename Request, typename LookupKey>
  struct resolve_request_status_check<Request, LookupKey,
                                      binding_status::ambiguous> {
    static_assert(
        resolve_status_v<Request, detail::make_lookup_key_t<LookupKey>> !=
            binding_status::ambiguous,
        "static_container cannot resolve an ambiguously bound type");
    using type = void;
  };

  template <typename Request, typename LookupKey>
  struct resolve_request_status_check<Request, LookupKey,
                                      binding_status::not_found> {
    static_assert(has_parent_v,
                  "static_container cannot resolve an unbound type");
    using type = void;
  };

  template <typename Request, typename R, typename LookupKey,
            bool IsCollection = collection_traits<R>::is_collection>
  struct resolve_request_check {
    using type = void;
  };

  template <typename Request, typename R, typename LookupKey>
  struct resolve_request_check<Request, R, LookupKey, false>
      : resolve_request_status_check<Request, LookupKey> {};

  template <typename Collection, typename Key>
  static constexpr std::size_t collection_count_v =
      static_collection_binding_count<static_bindings_type, Collection, Key>();

  // Keep the public static boundary out of static_context when the request is a
  // direct stored reference/pointer read. Those paths are part of the tiny
  // static codegen contract; any temporary, collection, conversion, or parent
  // path must use the real context instead.
  template <typename Request, typename Result, typename LookupKey>
  static constexpr bool uses_static_minimal_context_v = [] {
    if constexpr (collection_traits<Result>::is_collection ||
                  (has_parent_v && resolve_status_v<Request, LookupKey> ==
                                       binding_status::not_found)) {
      return false;
    } else {
      using selection = selection_t<typename Request::lookup_type, LookupKey>;
      return minimal_context_selection<Request, Result, selection>::value;
    }
  }();

  template <typename Request, typename Result, typename LookupKey>
  using static_resolution_context_t = std::conditional_t<
      uses_static_minimal_context_v<Request, Result, LookupKey>,
      no_dependency_context, context_type>;

  template <typename Request, typename Context, typename Origin,
            typename LookupKey,
            typename R = request_result_t<typename Request::user_type,
                                          Request::removes_rvalue_references>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(construction_scope scope, Context &context, Origin &origin,
                   LookupKey key) {
    using user_type = typename Request::user_type;
    return parent_
        ->template resolve<user_type, Request::removes_rvalue_references>(
            scope, context, origin, key);
  }

  // A request no binding of this container answers goes to the parent and is
  // reported unresolved when there is none.
  template <typename Request, typename R, typename Context, typename Origin,
            typename LookupKey>
  R resolve_missing(construction_scope scope, Context &context, Origin &origin,
                    LookupKey key) {
    if constexpr (has_parent_v) {
      if (parent_) {
        return resolve_parent<Request>(scope, context, origin, std::move(key));
      }
    }
    return origin
        .template unresolved<Request, false, LookupKey, R, !has_parent_v>(
            scope, context, key);
  }

public:
  using static_registry_type =
      static_registry<static_bindings_type, state_type>;
  using container_traits_type = ContainerTraits;
  using rtti_type = typename ContainerTraits::rtti_type;

  static_assert(static_bindings_type::valid,
                "static_container requires a valid compile-time bindings "
                "source");
  static_assert(detail::key_value_bindings_are_declared<
                    typename static_bindings_type::interface_bindings,
                    index_entries_>::value,
                "static_container fixed dingo::key_type<Key, Value> bindings "
                "require associative<Key, Interface, one> or "
                "associative<Key, Interface, many>");
  static_assert(detail::key_value_bindings_are_unique<
                    typename static_bindings_type::interface_bindings,
                    index_entries_>::value,
                "static_container fixed runtime-key lookup bindings must be "
                "unique for one lookups and unique by storage for many "
                "lookups");
  static_assert(graph_type_::resolvable,
                "static_container requires a resolvable compile-time binding "
                "graph");
  static_assert((binding_factory_is_default_constructible<
                     binding_model<Registrations>>::value &&
                 ...),
                "static_container requires default-constructible factories");
  static_assert((binding_storage_is_default_constructible<
                     binding_model<Registrations>>::value &&
                 ...),
                "static_container requires default-constructible storage "
                "objects");

  static_container_impl() = default;

  // Bindings keep their instances inline, and resolved instances reference
  // each other by address, so the container cannot move.
  static_container_impl(const static_container_impl &) = delete;
  static_container_impl(static_container_impl &&) = delete;
  static_container_impl &operator=(const static_container_impl &) = delete;
  static_container_impl &operator=(static_container_impl &&) = delete;

  template <typename Parent = ParentContainer,
            std::enable_if_t<!std::is_void_v<Parent>, int> = 0>
  explicit static_container_impl(Parent *parent)
      : parent_(container_implementation_of(parent)) {}

  template <typename T, typename Key = key_type<none_t>,
            typename R = request_result_t<T, true>>
  R resolve() {
    using lookup_key_type =
        decltype(detail::make_lookup_key(type_selector<Key>{}));
    return resolve<T, lookup_key_type, R>(
        detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename LookupKey,
            typename R = request_result_t<T, true>,
            typename = typename resolve_request_check<request_type<T, false>, R,
                                                      LookupKey>::type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(LookupKey key) {
    using lookup_request = request_type<T, true>;
    using request = request_type<T, false>;
    using policy =
        detail::resolution_policy<T, LookupKey, static_bindings_type,
                                  detail::resolution_mode::compile_time>;
    return policy::template resolve<R>(
        [&](auto) -> R {
          if constexpr (!collection_traits<R>::is_collection) {
            if constexpr (uses_static_minimal_context_v<request, R,
                                                        LookupKey>) {
              using selected =
                  typename static_registry_type::template selection<
                      typename lookup_request::lookup_type, LookupKey>;
              static_resolution_context_t<request, R, LookupKey> context;
              return static_registry_.template resolve_binding<
                  typename lookup_request::lookup_type, R, selected>(
                  ephemeral_scope, context, *this);
            }
          }
          if constexpr (collection_traits<R>::is_collection) {
            context_type context;
            return resolve_request<request, R>(ephemeral_scope, context, *this,
                                               key);
          } else {
            if constexpr (has_parent_v &&
                          resolve_status_v<request, LookupKey> ==
                              binding_status::not_found) {
              if (parent_) {
                context_type context;
                return resolve_parent<request>(ephemeral_scope, context, *this,
                                               key);
              }
            }
            context_type context;
            return resolve_request<request, R>(ephemeral_scope, context, *this,
                                               key);
          }
        },
        [&](auto tag) -> R {
          using selected_policy = typename decltype(tag)::policy;
          return selected_policy::lift_type::make(
              resolve<
                  typename selected_policy::inner_type, LookupKey,
                  request_result_t<typename selected_policy::inner_type, true>>(
                  key));
        });
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey,
            typename R = request_result_t<T, RemoveRvalueReferences>,
            typename = typename resolve_request_check<
                request_type<T, RemoveRvalueReferences>, R, LookupKey>::type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, Context &context, LookupKey key) {
    return resolve<T, RemoveRvalueReferences>(scope, context, *this,
                                              std::move(key));
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename Origin, typename LookupKey,
            typename R = request_result_t<T, RemoveRvalueReferences>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, Context &context, Origin &origin,
            LookupKey key) {
    using lookup_request = request_type<T, true>;
    using request = request_type<T, RemoveRvalueReferences>;
    using policy =
        detail::resolution_policy<T, LookupKey, static_bindings_type,
                                  detail::resolution_mode::compile_time>;
    return policy::template resolve<R>(
        [&](auto) -> R {
          if constexpr (!collection_traits<R>::is_collection) {
            using selection = typename static_bindings_type::template selection<
                typename lookup_request::lookup_type, LookupKey>;
            if constexpr (selection::status == binding_status::found) {
              using binding = typename selection::binding_type;
              using binding_model_type = typename binding::binding_model_type;
              if constexpr (binding_supports_request_v<R, binding> &&
                            (std::is_reference_v<R> || std::is_pointer_v<R>) &&
                            stored_request_identity_v<
                                R, typename binding_model_type::storage_type> &&
                            factory_without_dependencies_v<
                                typename binding_model_type::factory_type>) {
                using selected =
                    typename static_registry_type::template selection<
                        typename lookup_request::lookup_type, LookupKey>;
                return static_registry_.template resolve_binding<
                    typename lookup_request::lookup_type, R, selected>(
                    scope, context, *this);
              }
            }
          }
          if constexpr (collection_traits<R>::is_collection) {
            if constexpr (has_parent_v &&
                          collection_count_v<R, LookupKey> == 0) {
              return resolve_missing<request, R>(scope, context, origin,
                                                 std::move(key));
            } else {
              return resolve_request<request, R>(scope, context, origin, key);
            }
          } else {
            if constexpr (has_parent_v &&
                          resolve_status_v<request, LookupKey> ==
                              binding_status::not_found) {
              if (parent_) {
                return resolve_parent<request>(scope, context, origin, key);
              }
            }
            return resolve_request<request, R>(scope, context, origin, key);
          }
        },
        [&](auto tag) -> R {
          using selected_policy = typename decltype(tag)::policy;
          return selected_policy::lift_type::make(
              resolve<typename selected_policy::inner_type, false>(
                  scope, context, origin, key));
        });
  }

  template <typename T, typename IdType, typename R = request_result_t<T, true>,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  R resolve(IdType &&id) {
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    using lookup_key_type = decltype(key);
    if constexpr (detail::is_static_lookup_key_v<lookup_key_type>) {
      return resolve<T, lookup_key_type, R>(std::move(key));
    } else {
      static_assert(detail::container_dependent_false_v<T, lookup_key_type>,
                    "static_container fixed runtime-key request requires "
                    "dingo::key_type<Key, Value>");
    }
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T, true>>
  R construct(Factory factory = Factory()) {
    return construct_request<request_type<T>, Factory, R>(std::move(factory));
  }

private:
  template <typename Request,
            typename Factory = constructor<typename Request::value_type>,
            typename R = typename Request::result_type>
  R construct_request(Factory factory = Factory()) {
    using user_type = typename Request::user_type;
    using request_value_type = typename Request::value_type;
    using interface_type = typename Request::interface_type;
    if constexpr (std::is_same_v<Factory, constructor<request_value_type>>) {
      using selection = typename static_bindings_type::template selection<
          binding_dependency_interface_t<interface_type>,
          binding_dependency_key_t<interface_type>>;
      using normalized_selection =
          typename static_bindings_type::template selection<
              request_value_type, detail::no_lookup_key_t>;
      constexpr bool has_exact_binding =
          selection::status != binding_status::not_found;
      constexpr bool has_normalized_binding =
          normalized_selection::status != binding_status::not_found;
      if constexpr (has_exact_binding) {
        using binding = typename selection::binding_type;
        if constexpr (binding_supports_request_v<user_type, binding>) {
          return resolve<user_type>();
        }
      }

      if constexpr (has_exact_binding || has_normalized_binding) {
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          user_type>) {
          context_type context;
          ::dingo::throw_missing_rvalue_conversion<user_type>(
              has_normalized_binding, context);
        } else if constexpr (construct_normalized_request_v<user_type>) {
          return construct_static_binding_value<user_type,
                                                normalized_selection>(
              [&]() { return resolve<request_value_type>(); });
        } else {
          return resolve<user_type>();
        }
      } else {
        context_type context;
        auto type_guard = context.template track_type<request_value_type>();
        return factory.template construct<R>(ephemeral_scope, context, *this);
      }
    } else {
      context_type context;
      auto type_guard = context.template track_type<request_value_type>();
      return factory.template construct<R>(ephemeral_scope, context, *this);
    }
  }

public:
  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    context_type context;
    return detail::container_invoke<Signature>(
        context, *this, ephemeral_scope, std::forward<Callable>(callable));
  }

private:
  template <typename T, typename LookupKey, typename Context, typename... Args>
  T construct_collection_static(construction_scope scope, Context &context,
                                Args &&...args) {
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      return static_registry_.template construct_collection<T, LookupKey>(
          scope, *this, context, std::forward<Args>(args)...);
    } else {
      static_assert(detail::container_dependent_false_v<T, LookupKey>,
                    "static_container fixed runtime-key collection request "
                    "requires dingo::key_type<Key, Value>");
    }
  }

public:
  using container_api<
      static_container_impl<static_bindings<Registrations...>, ParentContainer,
                            ContainerTraits>>::append_collection;
  using container_api<
      static_container_impl<static_bindings<Registrations...>, ParentContainer,
                            ContainerTraits>>::construct_collection;
  using container_api<
      static_container_impl<static_bindings<Registrations...>, ParentContainer,
                            ContainerTraits>>::count_collection;

private:
  friend class container_api<static_container_impl<
      static_bindings<Registrations...>, ParentContainer, ContainerTraits>>;

  template <typename T, typename LookupKey>
  T construct_collection_by_key(LookupKey key) {
    context_type context;
    return construct_collection<T>(ephemeral_scope, context, std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey>
  T construct_collection_by_key(Fn &&fn, LookupKey key) {
    context_type context;
    return construct_collection<T>(ephemeral_scope, context,
                                   std::forward<Fn>(fn), std::move(key));
  }

public:
  template <typename T, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope, Context &context,
                         LookupKey) {
    return construct_collection_static<T, LookupKey>(scope, context);
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope, Context &context, Fn &&fn,
                         LookupKey) {
    return construct_collection_static<T, LookupKey>(scope, context,
                                                     std::forward<Fn>(fn));
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn, LookupKey) {
    return static_registry_.template append_collection<T, LookupKey>(
        scope, results, *this, context, std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey) {
    return static_registry_.template count_collection<T, LookupKey>();
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename R = request_result_t<T, RemoveRvalueReferences>,
            typename = typename resolve_request_check<
                request_type<T, RemoveRvalueReferences>, R,
                decltype(detail::no_lookup_key())>::type>
  R resolve(construction_scope scope, Context &context) {
    return resolve_request<request_type<T, RemoveRvalueReferences>, R>(
        scope, context, *this, detail::no_lookup_key());
  }

  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            bool StaticError = false, typename Context,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, Context &context, LookupKey key) {
    return detail::resolve_missing_request<detail::static_miss_policy,
                                           MayAutoConstruct, StaticError,
                                           Request, LookupKey, R>(
        *this, scope, context, std::move(key));
  }

private:
  template <typename Request, typename R, typename Context, typename Origin,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_request(construction_scope scope, Context &context, Origin &origin,
                    LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v && collection_count_v<R, LookupKey> == 0) {
        return resolve_missing<Request, R>(scope, context, origin,
                                           std::move(key));
      } else {
        return static_registry_.template construct_collection<R, LookupKey>(
            scope, *this, context);
      }
    } else {
      using lookup_type = typename Request::lookup_type;
      constexpr auto status = resolve_status_v<Request, LookupKey>;
      if constexpr (status == binding_status::found) {
        using selected =
            typename static_registry_type::template selection<lookup_type,
                                                              LookupKey>;
        return static_registry_.template resolve_binding<
            lookup_type, typename Request::interface_type, selected>(
            scope, context, *this);
      } else if constexpr (status == binding_status::ambiguous) {
        static_assert(status != binding_status::ambiguous,
                      "static_container cannot resolve an ambiguously bound "
                      "type");
      } else {
        return resolve_missing<Request, R>(scope, context, origin,
                                           std::move(key));
      }
    }
  }

private:
  static_registry_type static_registry_;
  parent_container_type *parent_ = nullptr;
};

} // namespace detail

template <typename StaticSource, typename ParentContainer>
class static_container
    : public detail::static_container_impl<
          static_bindings_source_t<StaticSource>,
          std::conditional_t<
              detail::is_static_container_traits_v<ParentContainer>, void,
              ParentContainer>,
          std::conditional_t<
              detail::is_static_container_traits_v<ParentContainer>,
              ParentContainer,
              detail::static_container_traits_t<ParentContainer>>> {
  using base_type = detail::static_container_impl<
      static_bindings_source_t<StaticSource>,
      std::conditional_t<detail::is_static_container_traits_v<ParentContainer>,
                         void, ParentContainer>,
      std::conditional_t<detail::is_static_container_traits_v<ParentContainer>,
                         ParentContainer,
                         detail::static_container_traits_t<ParentContainer>>>;

public:
  using base_type::base_type;
};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
