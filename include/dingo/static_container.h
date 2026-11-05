//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/binding/container_interface.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/type_registration.h>
#include <dingo/static/activation_set.h>
#include <dingo/static/container_traits.h>
#include <dingo/static/context.h>
#include <dingo/static/graph.h>
#include <dingo/static/registry.h>
#include <dingo/static/resolution.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

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
    : public container_interface<static_container_impl<
          static_bindings<Registrations...>, ParentContainer, ContainerTraits>>,
      private static_container_dependency_diagnostics_base<
          static_bindings<Registrations...>, !std::is_void_v<ParentContainer>> {
public:
  using static_bindings_type = static_bindings<Registrations...>;
  using context_static_bindings_type = static_bindings_cat_t<
      static_bindings_type,
      static_container_context_bindings_t<ParentContainer>>;

private:
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using graph_type_ = graph_analysis<static_bindings_type, has_parent_v>;
  using parent_container_type = container_implementation_t<ParentContainer>;
  using storage_type = static_binding_storage<Registrations...>;
  using context_type = static_context<context_static_bindings_type>;
  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<ContainerTraits>>;

  template <typename Request, typename Key>
  using selection_t =
      typename static_bindings_type::template selection<Request, Key>;

  template <typename Collection, typename Key>
  static constexpr std::size_t collection_count_v =
      static_collection_binding_count<static_bindings_type, Collection, Key>();

  // Keep the public static boundary out of static_context when the request is a
  // direct stored reference/pointer read. Those paths are part of the tiny
  // static codegen contract; any temporary, collection, conversion, or parent
  // path must use the real context instead.
  template <typename Request, typename Result, typename LookupKey>
  static constexpr bool uses_static_minimal_context_v = [] {
    using selection = selection_t<typename Request::lookup_type, LookupKey>;
    if constexpr (collection_traits<Result>::is_collection ||
                  selection::status != binding_status::found) {
      return false;
    } else {
      using binding = typename selection::binding_type;
      using model = typename binding::binding_model_type;
      using interface_type = typename Request::interface_type;
      return (std::is_reference_v<Result> || std::is_pointer_v<Result>) &&
             binding_supports_request_v<interface_type, binding> &&
             stored_request_identity_v<interface_type,
                                       typename model::storage_type> &&
             factory_without_dependencies_v<typename model::factory_type>;
    }
  }();

  template <typename Lookup, typename Result, typename LookupKey,
            typename Context>
  decltype(auto) resolve_bound(construction_scope scope, Context &context) {
    using binding = typename selection_t<Lookup, LookupKey>::binding_type;
    return storage_.template resolve<binding, Result>(scope, *this, context);
  }

public:
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
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(LookupKey key) {
    using lookup_request = request_type<T, true>;
    using request = request_type<T, false>;
    if constexpr (uses_static_minimal_context_v<request, R, LookupKey>) {
      no_dependency_context context;
      return resolve_bound<typename lookup_request::lookup_type, R, LookupKey>(
          ephemeral_scope, context);
    } else {
      context_type context;
      return request_resolution::resolve<request, R, false>(
          *this, ephemeral_scope, context, *this, key);
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename Context,
            typename LookupKey,
            typename R = request_result_t<T, RemoveRvalueReferences>,
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
    return request_resolution::resolve<request_type<T, RemoveRvalueReferences>,
                                       R, false>(*this, scope, context, origin,
                                                 std::move(key));
  }

  template <typename T, typename IdType, typename R = request_result_t<T, true>,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  R resolve(IdType &&id) {
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    using lookup_key_type = decltype(key);
    if constexpr (detail::is_static_lookup_key_v<lookup_key_type>) {
      return resolve<T, lookup_key_type, R>(std::move(key));
    } else {
      static_assert(detail::dependent_false_v<T, lookup_key_type>,
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
  using container_interface<static_container_impl>::construct_collection;
  using container_interface<static_container_impl>::count_collection;

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey) {
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      return with_context([&](context_type &context) -> T {
        if constexpr (std::is_same_v<std::decay_t<Fn>,
                                     binding_collection_append>) {
          return storage_.template construct_collection<T, LookupKey,
                                                        static_bindings_type>(
              ephemeral_scope, *this, context);
        } else {
          return storage_.template construct_collection<T, LookupKey,
                                                        static_bindings_type>(
              ephemeral_scope, *this, context, std::forward<Fn>(fn));
        }
      });
    } else {
      static_assert(detail::dependent_false_v<T, LookupKey>,
                    "static_container fixed runtime-key collection request "
                    "requires dingo::key_type<Key, Value>");
    }
  }

  template <typename Fn> decltype(auto) with_context(Fn &&fn) {
    context_type context;
    return std::forward<Fn>(fn)(context);
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                Context &context, Fn &&fn, LookupKey) {
    return storage_
        .template append_collection<T, LookupKey, static_bindings_type>(
            scope, results, *this, context, std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  auto count_collection(LookupKey) {
    return std::integral_constant<std::size_t,
                                  collection_count_v<T, LookupKey>>{};
  }

  // A request that no container of the chain binds. Without a parent, the
  // request of the static container itself cannot be bound at run time.
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            bool StaticError = !has_parent_v, typename Context,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, Context &context, LookupKey key) {
    (void)MayAutoConstruct;
    if constexpr (collection_traits<R>::is_collection) {
      (void)scope;
      (void)context;
      (void)key;
      static_assert(!StaticError || dependent_false_v<Request, LookupKey>,
                    "static container cannot construct a collection for an "
                    "unbound type");
      using resolve_type = typename collection_traits<R>::resolve_type;
      throw make_collection_type_not_found_exception<R, resolve_type>();
    } else {
      (void)scope;
      (void)context;
      (void)key;
      if constexpr (StaticError) {
        static_assert(dependent_false_v<Request, LookupKey>,
                      "static_container cannot resolve an unbound type");
      } else {
        throw make_type_not_found_exception<typename Request::lookup_type>();
      }
    }
  }

private:
  friend struct detail::request_resolution;

  template <typename Request, typename LookupKey>
  auto select_request(const LookupKey &) {
    return selection_t<typename Request::lookup_type, LookupKey>{};
  }

  template <typename Request, typename R, typename Selection, typename Context>
  decltype(auto) resolve_selected(Selection &, construction_scope scope,
                                  Context &context) {
    return storage_.template resolve<typename Selection::binding_type,
                                     typename Request::interface_type>(
        scope, *this, context);
  }

  template <typename Request, typename Context>
  [[noreturn]] void ambiguous(Context &context) {
    static_assert(dependent_false_v<Request, Context>,
                  "static_container cannot resolve an ambiguously bound type");
    throw make_type_ambiguous_exception<typename Request::lookup_type>(context);
  }

  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves() {
    return has_parent_v;
  }

  parent_container_type *parent() { return parent_; }

  template <typename R, typename Context, typename LookupKey>
  R construct_request_collection(construction_scope scope, Context &context,
                                 LookupKey) {
    return storage_
        .template construct_collection<R, LookupKey, static_bindings_type>(
            scope, *this, context);
  }

private:
  storage_type storage_;
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
