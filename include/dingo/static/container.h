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
#include <dingo/binding/requests.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/type_registration.h>
#include <dingo/static/binding.h>
#include <dingo/static/container_traits.h>
#include <dingo/static/context.h>
#include <dingo/static/diagnostics.h>
#include <dingo/static/graph.h>
#include <dingo/static/registry.h>
#include <dingo/type/annotated.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/request.h>
#include <dingo/type/shape.h>

#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

namespace detail {

struct static_container_no_dependency_diagnostics {};

template <typename StaticRegistry, bool HasParent>
struct static_container_dependency_diagnostics_base;

template <typename... Registrations>
struct static_container_dependency_diagnostics_base<
    static_registry<Registrations...>, false>
    : static_registry_dependency_diagnostics<
          typename static_registry<Registrations...>::interface_bindings,
          binding_model<Registrations>...> {};

template <typename... Registrations>
struct static_container_dependency_diagnostics_base<
    static_registry<Registrations...>, true>
    : static_container_no_dependency_diagnostics {};

template <typename StaticRegistry, typename ParentContainer = void,
          typename ContainerTraits = ::dingo::static_container_traits>
class static_container_impl;

// Whether the resolutions of a container carry persistent memory: those of a
// static container that has some, or whose static parent's resolutions do.
template <typename Container, typename = void>
struct uses_persistent_memory : std::false_type {};

template <typename Container>
struct uses_persistent_memory<
    Container, std::void_t<decltype(Container::uses_persistent_memory_v)>>
    : std::bool_constant<Container::uses_persistent_memory_v> {};

template <typename ParentContainer, typename ContainerTraits,
          typename... Registrations>
class static_container_impl<static_registry<Registrations...>, ParentContainer,
                            ContainerTraits>
    : public container_requests<static_container_impl<
          static_registry<Registrations...>, ParentContainer, ContainerTraits>>,
      private static_container_dependency_diagnostics_base<
          static_registry<Registrations...>, !std::is_void_v<ParentContainer>> {
public:
  using static_bindings_type = static_registry<Registrations...>;

private:
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using graph_type_ = graph_model<static_bindings_type>;
  using parent_container_type = container_implementation_t<ParentContainer>;
  using storage_type = static_bindings_state<Registrations...>;
  static constexpr bool has_persistent_memory_v =
      graph_type_::keeps_constructed_dependencies;

public:
  static constexpr bool uses_persistent_memory_v =
      has_persistent_memory_v ||
      uses_persistent_memory<parent_container_type>::value;

private:
  using context_type =
      std::conditional_t<uses_persistent_memory_v, static_persistent_context,
                         static_context>;
  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<ContainerTraits>>;

  template <typename Request, typename Key>
  using selection_t =
      typename static_bindings_type::template selection<Request, Key>;

  template <typename Collection, typename Key>
  static constexpr std::size_t collection_count_v =
      static_collection_binding_count<static_bindings_type, Collection, Key>();

  // Whether a request of the caller is served from a value placed in the
  // context, which ends before the caller uses it. Only an address request of
  // a scope that does not keep its instance asks for the type its storage
  // yields: that instantiates the factory, and so the dependency chain.
  template <typename Request, typename Result, typename LookupKey>
  static constexpr bool escapes_context_v = [] {
    using selection = selection_t<typename Request::lookup_type, LookupKey>;
    if constexpr (collection_traits<Result>::is_collection ||
                  (!std::is_lvalue_reference_v<Result> &&
                   !std::is_pointer_v<Result>) ||
                  selection::status != binding_status::found) {
      return false;
    } else {
      using model = typename selection::binding_type::binding_model_type;
      if constexpr (model::conversions_type::is_stable) {
        return false;
      } else {
        using activation = binding_activation<
            binding_cell_t<model>,
            typename static_dependencies<static_container_impl,
                                         typename model::bindings_type>::type>;
        return activation::template places_value_in_context<Result,
                                                            context_type>;
      }
    }
  }();

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
             factory_without_dependencies_v<typename model::factory_type> &&
             !escapes_context_v<Request, Result, LookupKey>;
    }
  }();

  template <typename Lookup, typename Result, typename LookupKey,
            typename Context, typename Scope>
  decltype(auto) resolve_bound(Scope scope, Context &context) {
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

  using container_requests<static_container_impl>::resolve;

  // A request a child container forwards here constructs what this container
  // keeps in the memory of this container, which outlives the child.
  template <typename Request, typename R, typename Context, typename Origin,
            typename LookupKey, typename Scope>
  R resolve_request(Scope scope, Context &context, Origin &origin,
                    LookupKey key) {
    if constexpr (std::is_same_v<Context, static_persistent_context> &&
                  has_persistent_memory_v) {
      if (static_cast<void *>(std::addressof(origin)) != this) {
        persistent_memory_scope memory(context, persistent_memory());
        return request_resolution::resolve<Request, R>(*this, scope, context,
                                                       origin, std::move(key));
      }
    }
    return request_resolution::resolve<Request, R>(*this, scope, context,
                                                   origin, std::move(key));
  }

public:
  using container_requests<static_container_impl>::construct_collection;
  using container_requests<static_container_impl>::count_collection;

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey) {
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      return with_context([&](context_type &context) -> T {
        if constexpr (std::is_same_v<std::decay_t<Fn>,
                                     binding_collection_append>) {
          return storage_.template construct_collection<T, LookupKey,
                                                        static_bindings_type>(
              caller_scope, *this, context);
        } else {
          return storage_.template construct_collection<T, LookupKey,
                                                        static_bindings_type>(
              caller_scope, *this, context, std::forward<Fn>(fn));
        }
      });
    } else {
      static_assert(detail::dependent_false_v<T, LookupKey>,
                    "static_container fixed runtime-key collection request "
                    "requires dingo::key_type<Key, Value>");
    }
  }

  template <typename Fn>
  DINGO_ALWAYS_INLINE decltype(auto) with_context(Fn &&fn) {
    context_type context(persistent_memory());
    return std::forward<Fn>(fn)(context);
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  auto count_collection(LookupKey) {
    return std::integral_constant<std::size_t,
                                  collection_count_v<T, LookupKey>>{};
  }

  // A request that no container of the chain binds. It is auto-constructed as
  // in the runtime container when it may be; without a parent, any other
  // request of the static container is rejected at compile time.
  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            bool StaticError = !has_parent_v, typename Context,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0,
            typename Scope>
  R unresolved(Scope scope, Context &context, LookupKey key) {
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
      using value_type = typename Request::value_type;
      constexpr bool constructible = [] {
        if constexpr (MayAutoConstruct &&
                      is_auto_constructible<
                          std::decay_t<typename Request::user_type>>::value) {
          return constructor<value_type>::kind == constructor_kind::concrete;
        } else {
          return false;
        }
      }();
      static_assert(!StaticError || constructible,
                    "static_container cannot resolve an unbound type");
      return request_resolution::unresolved<Request, MayAutoConstruct, R>(
          *this, *this, scope, context, key);
    }
  }

private:
  friend struct detail::request_resolution;
  friend class container_requests<static_container_impl>;

  template <typename Request, typename R, typename LookupKey>
  DINGO_ALWAYS_INLINE R resolve_entry(LookupKey key) {
    static_assert(detail::is_static_lookup_key_v<LookupKey>,
                  "static_container fixed runtime-key request requires "
                  "dingo::key_type<Key, Value>");
    static_assert(!escapes_context_v<Request, R, LookupKey>,
                  "static_container cannot return a reference or a pointer to "
                  "an instance the resolution constructs and does not keep; "
                  "request it by value, or as a dependency");
    if constexpr (uses_static_minimal_context_v<Request, R, LookupKey>) {
      no_dependency_context context;
      return resolve_bound<
          typename request_type<typename Request::user_type, true>::lookup_type,
          R, LookupKey>(caller_scope, context);
    } else {
      context_type context(persistent_memory());
      return request_resolution::resolve<Request, R>(*this, caller_scope,
                                                     context, *this, key);
    }
  }

  // The request itself if its binding publishes it, otherwise its normalized
  // value.
  template <typename Request, typename R, typename Selection, typename Context,
            typename Scope>
  R resolve_exact_or_normalized(Selection, Scope scope, Context &context) {
    constexpr bool exact = [] {
      if constexpr (Selection::status == binding_status::found) {
        return binding_supports_request_v<typename Request::user_type,
                                          typename Selection::binding_type>;
      } else {
        return false;
      }
    }();
    if constexpr (exact) {
      return request_resolution::resolve<Request, R>(*this, scope, context,
                                                     *this, no_lookup_key());
    } else {
      return request_resolution::resolve_normalized<Request, R>(*this, scope,
                                                                context);
    }
  }

  template <typename Request, typename LookupKey>
  auto select_request(const LookupKey &) {
    return selection_t<typename Request::lookup_type, LookupKey>{};
  }

  template <typename Request, typename R, typename Selection, typename Context,
            typename Scope>
  decltype(auto) resolve_selected(Selection &, Scope scope, Context &context) {
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

  template <typename R, typename Context, typename LookupKey, typename Scope>
  R construct_request_collection(Scope scope, Context &context, LookupKey) {
    return storage_
        .template construct_collection<R, LookupKey, static_bindings_type>(
            scope, *this, context);
  }

private:
  struct no_persistent_memory {};

  static_persistent_memory *persistent_memory() {
    if constexpr (has_persistent_memory_v) {
      return &persistent_;
    } else {
      return nullptr;
    }
  }

  // Only a container whose kept instances may refer to what is constructed for
  // them has the memory. It is declared before the storage, so that what they
  // refer to is destroyed after them.
  std::conditional_t<has_persistent_memory_v, static_persistent_memory,
                     no_persistent_memory>
      persistent_;
  storage_type storage_;
  parent_container_type *parent_ = nullptr;
};

} // namespace detail

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
