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
  template <typename T, typename Context, typename Container>
  friend T detail::resolve_context_request(Context &, Container &);

public:
  using static_bindings_type = static_bindings<Registrations...>;

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
  using context_type = static_context<static_bindings_type>;
  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<ContainerTraits>>;

  template <typename Collection, typename Fn>
  using collection_insert_invocable =
      std::is_invocable<Fn &, Collection &,
                        typename collection_traits<Collection>::resolve_type>;

  template <typename Request, typename Key>
  using selection_t = static_binding_t<
      typename static_bindings_type::template bindings<Request, Key>>;

  template <typename T, bool RemoveRvalueReferences, typename Key = void>
  static constexpr binding_status resolve_status_v =
      selection_t<resolve_dependency_t<T, RemoveRvalueReferences>, Key>::status;

  template <typename Collection, typename Key = void>
  static constexpr std::size_t collection_count_v =
      static_collection_binding_count<static_bindings_type, Collection, Key>();

  template <typename Collection, typename Key = void, typename R = Collection>
  R resolve_missing_collection() {
    if (parent_) {
      if constexpr (std::is_void_v<Key>) {
        return parent_->template resolve<Collection>();
      } else {
        return parent_->template resolve<Collection>(key_type<Key>{});
      }
    }
    using resolve_type = typename collection_traits<Collection>::resolve_type;
    throw make_collection_type_not_found_exception<Collection, resolve_type>();
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_dependency_result_t<T, RemoveRvalueReferences>>
  R resolve_parent() {
    if constexpr (std::is_void_v<Key>) {
      return parent_->template resolve<T>();
    } else {
      return parent_->template resolve<T>(key_type<Key>{});
    }
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

  template <typename T, typename Key = void,
            typename R = dependency_result_t<T>>
  R resolve(key_type<Key> = {}) {
    if constexpr (!collection_traits<R>::is_collection) {
      using lookup_request_type = resolve_dependency_t<T, true>;
      using request_type = R;
      using selection =
          static_binding_t<typename static_bindings_type::template bindings<
              lookup_request_type, Key>>;
      if constexpr (selection::status == binding_status::found) {
        using binding = typename selection::binding_type;
        using binding_model_type = typename binding::binding_model_type;
        if constexpr (binding_supports_request_v<request_type, binding> &&
                      (std::is_reference_v<request_type> ||
                       std::is_pointer_v<request_type>) &&
                      stored_request_identity_v<
                          request_type,
                          typename binding_model_type::storage_type> &&
                      factory_without_dependencies_v<
                          typename binding_model_type::factory_type>) {
          no_dependency_context context;
          using selected = typename static_registry_type::template selection<
              lookup_request_type, Key>;
          return static_registry_.template resolve_binding<
              lookup_request_type, request_type, selected>(context, *this);
        }
      }
    }
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v && collection_count_v<R, Key> == 0) {
        return resolve_missing_collection<R, Key>();
      } else {
        context_type context;
        return resolve<T, false, Key>(context);
      }
    } else {
      if constexpr (has_parent_v && resolve_status_v<T, false, Key> ==
                                        binding_status::not_found) {
        if (parent_) {
          return resolve_parent<T, false, Key>();
        }
      }
      context_type context;
      return resolve<T, false, Key>(context);
    }
  }

  template <typename T, typename IdType, typename R = dependency_result_t<T>,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  R resolve(IdType &&) {
    if constexpr (detail::is_key_value_v<IdType>) {
      return resolve<T>(key_type<std::decay_t<IdType>>{});
    } else {
      static_assert(detail::container_dependent_false_v<T, IdType>,
                    "static_container fixed runtime-key request requires "
                    "dingo::key_type<Key, Value>");
    }
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = dependency_result_t<T>>
  R construct(Factory factory = Factory()) {
    return construct_request<T>(std::move(factory));
  }

private:
  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = dependency_result_t<T>>
  R construct_request(Factory factory = Factory()) {
    using normalized_request_type = dependency_value_t<T>;
    using request_type = dependency_interface_t<T>;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      using selection =
          static_binding_t<typename static_bindings_type::template bindings<
              binding_dependency_interface_t<request_type>,
              binding_dependency_key_t<request_type>>>;
      using normalized_selection =
          static_binding_t<typename static_bindings_type::template bindings<
              normalized_request_type, void>>;
      constexpr bool has_exact_binding =
          selection::status != binding_status::not_found;
      constexpr bool has_normalized_binding =
          normalized_selection::status != binding_status::not_found;
      if constexpr (has_exact_binding) {
        using binding = typename selection::binding_type;
        if constexpr (binding_supports_request_v<T, binding>) {
          return resolve<T>();
        }
      }

      if constexpr (has_exact_binding || has_normalized_binding) {
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          context_type context;
          ::dingo::throw_missing_rvalue_conversion<T>(has_normalized_binding,
                                                      context);
        } else if constexpr (construct_normalized_request_v<T>) {
          return construct_static_binding_value<T, normalized_selection>(
              [&]() { return resolve<normalized_request_type>(); });
        } else {
          return resolve<T>();
        }
      } else {
        context_type context;
        auto type_guard =
            context.template track_type<normalized_request_type>();
        return factory.template construct<R>(context, *this);
      }
    } else {
      context_type context;
      auto type_guard = context.template track_type<normalized_request_type>();
      return factory.template construct<R>(context, *this);
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
        std::forward<Callable>(callable), context, *this);
  }

  template <typename T> T construct_collection() {
    context_type context;
    return static_registry_.template construct_collection<T>(*this, context);
  }

  template <typename T, typename Fn,
            std::enable_if_t<
                !detail::is_key_value_v<Fn> &&
                    collection_insert_invocable<T, std::decay_t<Fn>>::value,
                int> = 0>
  T construct_collection(Fn &&fn) {
    context_type context;
    return static_registry_.template construct_collection<T>(
        *this, context, std::forward<Fn>(fn));
  }

  template <typename T, typename Key> T construct_collection(key_type<Key>) {
    context_type context;
    return static_registry_.template construct_collection<T, Key>(*this,
                                                                  context);
  }

  template <typename T, typename IdType,
            std::enable_if_t<detail::is_key_value_v<IdType>, int> = 0>
  T construct_collection(IdType &&) {
    return construct_collection<T>(key_type<std::decay_t<IdType>>{});
  }

  template <
      typename T, typename IdType,
      std::enable_if_t<
          !is_none_v<std::decay_t<IdType>> && !detail::is_typed_key_v<IdType> &&
              !detail::is_key_value_v<IdType> &&
              !collection_insert_invocable<T, std::decay_t<IdType>>::value,
          int> = 0>
  T construct_collection(IdType &&) {
    static_assert(detail::container_dependent_false_v<T, IdType>,
                  "static_container fixed runtime-key collection request "
                  "requires dingo::key_type<Key, Value>");
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key_type<Key>) {
    context_type context;
    return static_registry_.template construct_collection<T, Key>(
        *this, context, std::forward<Fn>(fn));
  }

  template <typename T, typename Key = void, typename Fn, typename Context>
  std::size_t append_collection(T &results, Context &context, Fn &&fn) {
    return static_registry_.template append_collection<T, Key>(
        results, *this, context, std::forward<Fn>(fn));
  }

  template <typename T, typename Key = void> std::size_t count_collection() {
    return static_registry_.template count_collection<T, Key>();
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_dependency_result_t<T, RemoveRvalueReferences>>
  R resolve(context_type &context) {
    return resolve_request<T, RemoveRvalueReferences, Key>(context);
  }

private:
  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_dependency_result_t<T, RemoveRvalueReferences>>
  R resolve_request(context_type &context) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v && collection_count_v<R, Key> == 0) {
        return resolve_missing_collection<R, Key>();
      } else {
        return static_registry_.template construct_collection<R, Key>(*this,
                                                                      context);
      }
    } else {
      using lookup_request_type =
          resolve_dependency_t<T, RemoveRvalueReferences>;
      using request_type = R;
      constexpr auto status = resolve_status_v<T, RemoveRvalueReferences, Key>;
      if constexpr (has_parent_v) {
        if constexpr (status == binding_status::not_found) {
          if (parent_) {
            return resolve_parent<T, RemoveRvalueReferences, Key>();
          }
        }
      }
      if constexpr (status == binding_status::found) {
        using selected = typename static_registry_type::template selection<
            lookup_request_type, Key>;
        return static_registry_.template resolve_binding<
            lookup_request_type, request_type, selected>(context, *this);
      } else if constexpr (status == binding_status::ambiguous) {
        static_assert(status != binding_status::ambiguous,
                      "static_container cannot resolve an ambiguously bound "
                      "type");
      } else {
        static_assert(has_parent_v,
                      "static_container cannot resolve an unbound type");
        throw make_type_not_found_exception<lookup_request_type>();
      }
    }
  }

public:
  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(context_type &context, key_type<Key>) {
    return resolve_request<T, RemoveRvalueReferences, Key>(context);
  }

  template <typename T, bool RemoveRvalueReferences, typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  R resolve(context_type &context, IdType &&) {
    if constexpr (detail::is_key_value_v<IdType>) {
      return resolve_request<T, RemoveRvalueReferences, std::decay_t<IdType>>(
          context);
    } else {
      static_assert(detail::container_dependent_false_v<T, IdType>,
                    "dingo::dependency<T, dingo::key_type<Key, Value>> "
                    "constructor injection requires a fixed dependency key");
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
