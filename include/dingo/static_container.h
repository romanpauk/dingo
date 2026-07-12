//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/factory/invoke.h>
#include <dingo/static/activation_set.h>
#include <dingo/static/container_traits.h>
#include <dingo/static/context.h>
#include <dingo/static/resolution.h>
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
          typename ContainerTraits = ::dingo::static_container_traits<>>
class static_container_impl;

template <typename ParentContainer, typename ContainerTraits,
          typename... Registrations>
class static_container_impl<static_bindings<Registrations...>, ParentContainer,
                            ContainerTraits>
    : private static_container_dependency_diagnostics_base<
          static_bindings<Registrations...>, !std::is_void_v<ParentContainer>> {
public:
  using static_bindings_type = static_bindings<Registrations...>;
  using context_static_bindings_type = static_bindings_cat_t<
      static_bindings_type,
      static_container_context_bindings_t<ParentContainer>>;

private:
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using graph_type_ = std::conditional_t<
      has_parent_v, graph_analysis<static_bindings_type, true>,
      typename static_container_graph_type<
          static_bindings_type,
          static_bindings_type::dependencies_are_resolved>::type>;
  using self_type = static_container_impl<static_bindings_type, ParentContainer,
                                          ContainerTraits>;
  using parent_container_type = ParentContainer;
  using state_type = static_storage_state<Registrations...>;
  using context_type = static_context<context_static_bindings_type>;
  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<ContainerTraits>>;

  template <typename Collection, typename Fn>
  using collection_insert_invocable =
      std::is_invocable<Fn &, Collection &,
                        typename collection_traits<Collection>::resolve_type>;

  template <typename Request, typename Key>
  using selection_t = static_binding_t<
      typename static_bindings_type::template bindings<Request, Key>>;

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
      using selection =
          static_binding_t<typename static_bindings_type::template bindings<
              typename Request::lookup_type, LookupKey>>;
      return minimal_context_selection<Request, Result, selection>::value;
    }
  }();

  template <typename Request, typename Result, typename LookupKey>
  using static_resolution_context_t = std::conditional_t<
      uses_static_minimal_context_v<Request, Result, LookupKey>,
      no_dependency_context, context_type>;

  template <typename Collection, typename Context, typename LookupKey,
            typename R = Collection,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_missing_collection(construction_scope scope, Context &context,
                               LookupKey key) {
    if (parent_) {
      return parent_->template resolve<Collection, false>(scope, context, key);
    }
    using resolve_type = typename collection_traits<Collection>::resolve_type;
    throw make_collection_type_not_found_exception<Collection, resolve_type>();
  }

  template <typename Request, typename Context, typename LookupKey,
            typename R = typename request_type<
                typename Request::user_type,
                Request::removes_rvalue_references>::result_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(construction_scope scope, Context &context, LookupKey key) {
    using user_type = typename Request::user_type;
    return parent_
        ->template resolve<user_type, Request::removes_rvalue_references>(
            scope, context, key);
  }

public:
  using static_registry_type =
      static_registry<static_bindings_type, state_type>;
  using container_traits_type = ContainerTraits;
  using rtti_type = typename static_binding_scope<Registrations...>::rtti_type;

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

  template <typename Parent = ParentContainer,
            std::enable_if_t<!std::is_void_v<Parent>, int> = 0>
  explicit static_container_impl(Parent *parent) : parent_(parent) {}

  template <typename T, typename Key = key_type<none_t>,
            typename R = typename request_type<T, true>::result_type>
  R resolve() {
    using lookup_key_type =
        decltype(detail::make_lookup_key(type_selector<Key>{}));
    return resolve<T, lookup_key_type, R>(
        detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename LookupKey,
            typename R = typename request_type<T, true>::result_type,
            typename = typename resolve_request_check<request_type<T, false>, R,
                                                      LookupKey>::type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(LookupKey key) {
    using lookup_request = request_type<T, true>;
    using request = request_type<T, false>;
    if constexpr (!collection_traits<R>::is_collection) {
      if constexpr (uses_static_minimal_context_v<request, R, LookupKey>) {
        using selected = typename static_registry_type::template selection<
            typename lookup_request::lookup_type, LookupKey>;
        static_resolution_context_t<request, R, LookupKey> context;
        return static_registry_.template resolve_binding<
            typename lookup_request::lookup_type, R, selected>(ephemeral_scope,
                                                               context, *this);
      }
    }
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v && collection_count_v<R, LookupKey> == 0) {
        context_type context;
        return resolve_missing_collection<R>(ephemeral_scope, context, key);
      } else {
        context_type context;
        return resolve_request<request, R>(ephemeral_scope, context, key);
      }
    } else {
      if constexpr (has_parent_v && resolve_status_v<request, LookupKey> ==
                                        binding_status::not_found) {
        if (parent_) {
          context_type context;
          return resolve_parent<request>(ephemeral_scope, context, key);
        }
      }
      context_type context;
      return resolve_request<request, R>(ephemeral_scope, context, key);
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::result_type,
            typename = typename resolve_request_check<
                request_type<T, RemoveRvalueReferences>, R, LookupKey>::type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(construction_scope scope, Context &context, LookupKey key) {
    using lookup_request = request_type<T, true>;
    using request = request_type<T, RemoveRvalueReferences>;
    if constexpr (!collection_traits<R>::is_collection) {
      using selection =
          static_binding_t<typename static_bindings_type::template bindings<
              typename lookup_request::lookup_type, LookupKey>>;
      if constexpr (selection::status == binding_status::found) {
        using binding = typename selection::binding_type;
        using binding_model_type = typename binding::binding_model_type;
        if constexpr (binding_supports_request_v<R, binding> &&
                      (std::is_reference_v<R> || std::is_pointer_v<R>) &&
                      stored_request_identity_v<
                          R, typename binding_model_type::storage_type> &&
                      factory_without_dependencies_v<
                          typename binding_model_type::factory_type>) {
          using selected = typename static_registry_type::template selection<
              typename lookup_request::lookup_type, LookupKey>;
          return static_registry_.template resolve_binding<
              typename lookup_request::lookup_type, R, selected>(scope, context,
                                                                 *this);
        }
      }
    }
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v && collection_count_v<R, LookupKey> == 0) {
        return resolve_missing_collection<R>(scope, context, key);
      } else {
        return resolve_request<request, R>(scope, context, key);
      }
    } else {
      if constexpr (has_parent_v && resolve_status_v<request, LookupKey> ==
                                        binding_status::not_found) {
        if (parent_) {
          return resolve_parent<request>(scope, context, key);
        }
      }
      return resolve_request<request, R>(scope, context, key);
    }
  }

  template <typename T, typename IdType,
            typename R = typename request_type<T, true>::result_type,
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
            typename R = typename request_type<T, true>::result_type>
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
      using selection =
          static_binding_t<typename static_bindings_type::template bindings<
              binding_dependency_interface_t<interface_type>,
              binding_dependency_key_t<interface_type>>>;
      using normalized_selection =
          static_binding_t<typename static_bindings_type::template bindings<
              request_value_type, detail::no_lookup_key_t>>;
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
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        callable_dispatch_signature_t<Signature, callable_type>;

    context_type context;
    auto type_guard = context.template track_type<callable_type>();
    return callable_invoke<dispatch_signature>::construct(
        std::forward<Callable>(callable), ephemeral_scope, context, *this);
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
  template <typename T> T construct_collection() {
    context_type context;
    return construct_collection<T>(ephemeral_scope, context,
                                   detail::no_lookup_key());
  }

  template <typename T, typename Fn,
            std::enable_if_t<
                !detail::is_lookup_key_v<std::decay_t<Fn>> &&
                    !detail::is_key_value_v<Fn> &&
                    collection_insert_invocable<T, std::decay_t<Fn>>::value,
                int> = 0>
  T construct_collection(Fn &&fn) {
    context_type context;
    return construct_collection<T>(ephemeral_scope, context,
                                   std::forward<Fn>(fn),
                                   detail::no_lookup_key());
  }

  template <typename T, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope, Context &context,
                         LookupKey) {
    return construct_collection_static<T, LookupKey>(scope, context);
  }

  template <typename T, typename IdType,
            std::enable_if_t<!detail::is_lookup_key_v<IdType> &&
                                 !collection_insert_invocable<
                                     T, std::decay_t<IdType>>::value,
                             int> = 0>
  T construct_collection(IdType &&id) {
    context_type context;
    return construct_collection<T>(
        ephemeral_scope, context,
        detail::make_lookup_key(std::forward<IdType>(id)));
  }

  template <typename T, typename Fn, typename IdType,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  T construct_collection(Fn &&fn, IdType &&id) {
    context_type context;
    return construct_collection<T>(
        ephemeral_scope, context, std::forward<Fn>(fn),
        detail::make_lookup_key(std::forward<IdType>(id)));
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope, Context &context, Fn &&fn,
                         LookupKey) {
    return construct_collection_static<T, LookupKey>(scope, context,
                                                     std::forward<Fn>(fn));
  }

  template <typename T, typename Fn, typename Context>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn) {
    return append_collection<T>(scope, results, context, std::forward<Fn>(fn),
                                detail::no_lookup_key());
  }

  template <typename T, typename Key, typename Fn, typename Context>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn) {
    return append_collection<T>(scope, results, context, std::forward<Fn>(fn),
                                detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn, LookupKey) {
    return static_registry_.template append_collection<T, LookupKey>(
        scope, results, *this, context, std::forward<Fn>(fn));
  }

  template <typename T> std::size_t count_collection() {
    return count_collection<T>(detail::no_lookup_key());
  }

  template <typename T, typename Key> std::size_t count_collection() {
    return count_collection<T>(detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey) {
    return static_registry_.template count_collection<T, LookupKey>();
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::result_type,
            typename = typename resolve_request_check<
                request_type<T, RemoveRvalueReferences>, R,
                decltype(detail::no_lookup_key())>::type>
  R resolve(construction_scope scope, Context &context) {
    return resolve_request<request_type<T, RemoveRvalueReferences>, R>(
        scope, context, detail::no_lookup_key());
  }

private:
  template <typename Request, typename R, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_request(construction_scope scope, Context &context, LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v && collection_count_v<R, LookupKey> == 0) {
        return resolve_missing_collection<R>(scope, context, std::move(key));
      } else {
        return static_registry_.template construct_collection<R, LookupKey>(
            scope, *this, context);
      }
    } else {
      using lookup_type = typename Request::lookup_type;
      constexpr auto status = resolve_status_v<Request, LookupKey>;
      if constexpr (has_parent_v) {
        if constexpr (status == binding_status::not_found) {
          if (parent_) {
            return resolve_parent<Request>(scope, context, std::move(key));
          }
        }
      }
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
        static_assert(has_parent_v,
                      "static_container cannot resolve an unbound type");
        throw make_type_not_found_exception<lookup_type>();
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
