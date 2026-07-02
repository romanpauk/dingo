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
#include <dingo/core/exceptions.h>
#include <dingo/core/keyed.h>
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
#include <dingo/resolution/type_cache.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime_container.h>
#include <dingo/static/container_traits.h>
#include <dingo/static/context.h>
#include <dingo/static/local_resolution.h>
#include <dingo/static/resolution.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/complete_type.h>
#include <dingo/type/normalized_type.h>
#include <dingo/type/request_traits.h>
#include <dingo/type/type_map.h>

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
template <typename ParentContainer, typename... Registrations>
class detail::container_with_static_bindings<static_registry<Registrations...>,
                                             ParentContainer>
    : public detail::runtime_registration_api<
          detail::container_with_static_bindings<
              static_registry<Registrations...>, ParentContainer>> {
  friend class runtime_context;
  template <typename T, typename Context, typename Container>
  friend T detail::resolve_context_request(Context &, Container &);

  using static_registry_type_ = static_registry<Registrations...>;
  using self_type =
      detail::container_with_static_bindings<static_registry_type_,
                                             ParentContainer>;
  using runtime_base =
      runtime_registry<dynamic_container_traits,
                       typename dynamic_container_traits::allocator_type,
                       self_type, self_type>;
  using static_state = detail::static_storage_state<Registrations...>;
  using static_resolution_ref =
      detail::static_binding_scope_ref<static_state, Registrations...>;
  using binding_resolution_ref =
      detail::binding_scope_ref<static_state, Registrations...>;
  using static_context_type = static_context<static_registry_type_>;
  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<dynamic_container_traits>>;
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using parent_container_type = ParentContainer;

  template <typename T, typename Key>
  using static_selection_t = detail::static_binding_t<
      typename static_registry_type_::template bindings<T, Key>>;
  template <typename Key>
  using collection_key_t =
      std::conditional_t<std::is_void_v<Key>, none_t, key<Key>>;

  template <typename Key> static collection_key_t<Key> collection_key() {
    return {};
  }

  template <typename T>
  static constexpr bool runtime_auto_constructible_v =
      std::is_same_v<request_value_t<T>, std::decay_t<T>> &&
      (!std::is_reference_v<T> ||
       (std::is_lvalue_reference_v<T> &&
        std::is_const_v<std::remove_reference_t<T>> &&
        is_auto_constructible<std::decay_t<T>>::value));

  template <typename T, bool CheckCache, typename IdType>
  struct selected_runtime_binding {
    runtime_base &registry;
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
    self_type &self;
    IdType &id;

    template <typename Request>
    request_interface_t<Request> resolve(runtime_context &context) {
      return self.template resolve_missing_runtime<
          T, RemoveRvalueReferences, MayAutoConstruct, IdType,
          request_interface_t<Request>>(context, std::forward<IdType>(id));
    }
  };

  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename Request>
  struct runtime_binding_candidate {
    static constexpr bool can_resolve = true;

    self_type &self;

    detail::binding_selection_status status() const {
      return self.runtime_registry_.template binding_status<Request, Key>();
    }

    template <typename ResolveRequest>
    decltype(auto) resolve(runtime_context &context) {
      return self.template resolve_runtime_only<
          T, RemoveRvalueReferences, runtime_auto_constructible_v<T>>(
          context, collection_key<Key>());
    }
  };

  template <typename Request, typename Key> struct static_binding_candidate {
    using selection = detail::static_binding_t<
        typename static_registry_type_::template bindings<Request, Key>>;
    static constexpr bool can_resolve =
        selection::status == detail::binding_selection_status::found;

    self_type &self;

    constexpr detail::binding_selection_status status() const {
      return selection::status;
    }

    template <typename ResolveRequest>
    decltype(auto) resolve(runtime_context &context) {
      return self.template resolve_binding_selection<Request, Key>(context);
    }
  };

  template <typename Request, typename Key = void> bool has_runtime_binding() {
    return runtime_registry_.template binding_status<Request, Key>() !=
           detail::binding_selection_status::not_found;
  }

  template <typename T, typename Key = void> bool has_runtime_collection() {
    return runtime_registry_.template count_runtime_collection<T>(
               collection_key<Key>()) != 0;
  }

  template <typename Request, typename Key = void>
  static constexpr bool has_static_binding_v =
      static_selection_t<Request, Key>::status ==
      detail::binding_selection_status::found;

  template <typename Request, typename Key = void,
            bool Selected = has_static_binding_v<Request, Key>>
  struct static_binding_satisfies_request : std::false_type {};

  template <typename Request, typename Key>
  struct static_binding_satisfies_request<Request, Key, true>
      : std::bool_constant<
            (has_parent_v ||
             detail::static_binding_resolvable_v<
                 typename static_selection_t<Request, Key>::binding_type,
                 static_registry_type_>) &&
            detail::binding_supports_request_v<
                request_interface_t<Request>,
                typename static_selection_t<Request, Key>::binding_type>> {};

  template <typename Request, typename Key = void>
  static constexpr bool static_binding_satisfies_request_v =
      static_binding_satisfies_request<Request, Key>::value;

  template <typename Request, typename Key = void>
  bool select_static_resolve() {
    if constexpr (!static_binding_satisfies_request_v<Request, Key>) {
      return false;
    } else {
      return !has_runtime_binding<Request, Key>();
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void>
  static constexpr bool has_static_resolve_request_v =
      static_binding_satisfies_request_v<
          resolve_request_t<T, RemoveRvalueReferences>, Key>;

  template <typename T, bool RemoveRvalueReferences, typename Key = void>
  static constexpr detail::binding_selection_status static_resolve_status_v =
      static_selection_t<resolve_request_t<T, RemoveRvalueReferences>,
                         Key>::status;

  template <typename Collection, typename Key = void>
  bool select_static_collection() {
    if constexpr (!has_static_collection_v<Collection, Key>) {
      return false;
    } else {
      return !has_runtime_collection<Collection, Key>();
    }
  }

  template <typename T>
  static constexpr bool has_static_construct_request_v = [] {
    using request_type = request_interface_t<T>;
    using normalized_request_type = request_value_t<T>;
    constexpr bool has_exact_static_binding =
        static_binding_satisfies_request_v<request_type>;
    constexpr bool has_normalized_static_binding =
        static_selection_t<normalized_request_type, void>::status ==
            detail::binding_selection_status::found &&
        detail::static_binding_resolvable_v<
            typename static_selection_t<normalized_request_type,
                                        void>::binding_type,
            static_registry_type_>;

    return has_exact_static_binding ||
           (has_normalized_static_binding && construct_normalized_request_v<T>);
  }();

  template <typename T> bool select_static_construct() {
    using request_type = request_interface_t<T>;
    using normalized_request_type = request_value_t<T>;

    if constexpr (!has_static_construct_request_v<T>) {
      return false;
    } else {
      return !has_runtime_binding<request_type>() &&
             !has_runtime_binding<normalized_request_type>();
    }
  }

  template <typename T, typename Key = void>
  bool select_static_collection_construct() {
    constexpr bool has_static_collection_bindings =
        detail::static_collection_binding_count<static_registry_type_, T,
                                                Key>() != 0;
    if constexpr (!has_static_collection_bindings) {
      return false;
    } else {
      return !has_runtime_collection<T, Key>();
    }
  }

  template <typename Collection, typename Key = void>
  static constexpr bool has_static_collection_v =
      detail::static_collection_binding_count<static_registry_type_, Collection,
                                              Key>() != 0 &&
      detail::static_bindings_resolvable_v<
          typename static_registry_type_::template bindings<
              normalized_type_t<
                  typename collection_traits<Collection>::resolve_type>,
              Key>,
          static_registry_type_>;

  template <typename T, typename Key, typename Fn, typename Context>
  T construct_static_collection(Context &context, Fn &&fn, key<Key>) {
    static_resolution_ref static_state_ref(static_state_);
    return static_state_ref
        .template construct_static_collection<T, Key, static_registry_type_>(
            *this, context, std::forward<Fn>(fn));
  }

  template <typename T, typename Fn, typename Context>
  T construct_static_collection(Context &context, Fn &&fn, none_t) {
    static_resolution_ref static_state_ref(static_state_);
    return static_state_ref
        .template construct_static_collection<T, void, static_registry_type_>(
            *this, context, std::forward<Fn>(fn));
  }

  template <typename T, typename Key, typename Context>
  T construct_static_collection(Context &context, key<Key>) {
    static_resolution_ref static_state_ref(static_state_);
    return static_state_ref
        .template construct_static_collection<T, Key, static_registry_type_>(
            *this, context);
  }

  template <typename T, typename Context>
  T construct_static_collection(Context &context, none_t) {
    static_resolution_ref static_state_ref(static_state_);
    return static_state_ref
        .template construct_static_collection<T, void, static_registry_type_>(
            *this, context);
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = request_result_t<T>,
            std::enable_if_t<std::is_rvalue_reference_v<T>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve_static() {
    static_context_type static_context;
    return resolve_static<T, RemoveRvalueReferences, Key>(static_context);
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            std::enable_if_t<!std::is_rvalue_reference_v<T>, int> = 0>
  DINGO_ALWAYS_INLINE decltype(auto) resolve_static() {
    static_context_type static_context;
    return resolve_static<T, RemoveRvalueReferences, Key>(static_context);
  }

  template <typename T, typename R = request_result_t<T>>
  DINGO_ALWAYS_INLINE R construct_static() {
    using request_type = request_interface_t<T>;
    using normalized_request_type = request_value_t<T>;
    constexpr bool has_exact_static_binding =
        static_binding_satisfies_request_v<request_type>;
    constexpr bool has_normalized_static_binding =
        static_binding_satisfies_request_v<normalized_request_type>;
    if constexpr (has_exact_static_binding) {
      using binding =
          typename static_selection_t<request_type, void>::binding_type;
      if constexpr (detail::binding_supports_request_v<T, binding>) {
        static_context_type static_context;
        return resolve_static<T, false>(static_context);
      }
    }

    if constexpr (has_normalized_static_binding &&
                  construct_normalized_request_v<T>) {
      using normalized_selection =
          static_selection_t<normalized_request_type, void>;
      return detail::construct_static_binding_value<T, normalized_selection>(
          [&]() { return resolve_static<normalized_request_type, false>(); });
    } else {
      static_context_type static_context;
      return resolve_static<T, false>(static_context);
    }
  }

  template <
      typename T, typename Fn,
      std::enable_if_t<!detail::is_static_context_argument_v<Fn>, int> = 0>
  T construct_static_collection(Fn &&fn, none_t) {
    static_context_type static_context;
    return construct_static_collection<T>(static_context, std::forward<Fn>(fn),
                                          none_t{});
  }

  template <typename T> T construct_static_collection(none_t) {
    static_context_type static_context;
    return construct_static_collection<T>(static_context, none_t{});
  }

  template <
      typename T, typename Fn, typename Key,
      std::enable_if_t<!detail::is_static_context_argument_v<Fn>, int> = 0>
  T construct_static_collection(Fn &&fn, key<Key>) {
    static_context_type static_context;
    return construct_static_collection<T>(static_context, std::forward<Fn>(fn),
                                          key<Key>{});
  }

  template <typename T, typename Key> T construct_static_collection(key<Key>) {
    static_context_type static_context;
    return construct_static_collection<T>(static_context, key<Key>{});
  }

  template <typename T, typename Key, typename Fn>
  T construct_collection_impl(runtime_context &context, Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;
    binding_resolution_ref static_state_ref(static_state_);

    constexpr std::size_t static_count =
        type_list_size_v<typename static_registry_type_::template bindings<
            normalized_type_t<resolve_type>, Key>>;
    return detail::construct_binding_collection<T>(
        [&] {
          return runtime_registry_.template count_runtime_collection<T>(
              collection_key<Key>());
        },
        [&] { return static_count; },
        [&](auto &results, auto &&append) {
          runtime_registry_.append_runtime_collection(
              results, context, std::forward<decltype(append)>(append),
              collection_key<Key>());
        },
        [&](auto &results, auto &&append) {
          static_state_ref
              .template append_static_collection<T, Key, static_registry_type_>(
                  results, *this, context,
                  std::forward<decltype(append)>(append));
        },
        std::forward<Fn>(fn));
  }

  template <typename T, typename Key, typename Fn>
  T construct_collection(runtime_context &context, Fn &&fn, key<Key>) {
    return construct_collection_impl<T, Key>(context, std::forward<Fn>(fn));
  }

  template <typename T, typename Fn>
  T construct_collection(runtime_context &context, Fn &&fn, none_t) {
    return construct_collection_impl<T, void>(context, std::forward<Fn>(fn));
  }

  template <typename LookupRequest, typename Request, typename Key,
            typename Context>
  DINGO_ALWAYS_INLINE request_interface_t<Request>
  resolve_static_selection(Context &context) {
    using selection = static_selection_t<LookupRequest, Key>;
    using binding = typename selection::binding_type;
    if constexpr (selection::status !=
                  detail::binding_selection_status::found) {
      if constexpr (selection::status ==
                    detail::binding_selection_status::ambiguous) {
        throw detail::make_type_ambiguous_exception<LookupRequest>(context);
      } else {
        throw detail::make_type_not_found_exception<LookupRequest>(context);
      }
    } else {
      static_resolution_ref static_state_ref(static_state_);
      auto resolver =
          static_state_ref.template make_binding_resolver<binding>(*this);
      return resolver.template resolve<Request>(context);
    }
  }

  template <typename Request, typename Key, typename Context>
  request_interface_t<Request> resolve_binding_selection(Context &context) {
    using selection = static_selection_t<Request, Key>;
    using binding = typename selection::binding_type;
    if constexpr (selection::status !=
                  detail::binding_selection_status::found) {
      if constexpr (selection::status ==
                    detail::binding_selection_status::ambiguous) {
        throw detail::make_type_ambiguous_exception<Request>(context);
      } else {
        throw detail::make_type_not_found_exception<Request>(context);
      }
    } else {
      binding_resolution_ref static_state_ref(static_state_);
      auto resolver =
          static_state_ref.template make_binding_resolver<binding>(*this);
      return resolver.template resolve<Request>(context);
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename Context,
            typename R = resolve_result_t<T, RemoveRvalueReferences>>
  DINGO_ALWAYS_INLINE R resolve_static(Context &context) {
    if constexpr (collection_traits<R>::is_collection) {
      return construct_static_collection<R>(context, collection_key<Key>());
    } else {
      using lookup_request_type = resolve_request_t<T, RemoveRvalueReferences>;
      using request_type = R;
      return resolve_static_selection<lookup_request_type, request_type, Key>(
          context);
    }
  }

  template <typename T, typename Fn, typename IdType>
#if defined(__GNUC__) && !defined(__clang__)
  __attribute__((noinline))
#endif
  T construct_collection_runtime_context(Fn &&fn, IdType id) {
    runtime_context context;
    return construct_collection<T>(context, std::forward<Fn>(fn), id);
  }

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve_missing_runtime(runtime_context &context, IdType &&id) {
    using Type = normalized_type_t<T>;
    (void)id;

    if constexpr (MayAutoConstruct && detail::is_typed_key_v<IdType> &&
                  collection_traits<R>::is_collection) {
      return construct_collection_runtime_context<R>(
          detail::binding_collection_append{}, std::decay_t<IdType>{});
    } else if constexpr (MayAutoConstruct &&
                         is_auto_constructible<std::decay_t<T>>::value) {
      if constexpr (constructor<Type>::kind ==
                    detail::constructor_kind::concrete) {
        static_assert(is_complete<Type>::value,
                      "auto-construction requires a complete type");
        using type_detection = detail::automatic;
        return context.template construct_temporary<request_interface_t<T>,
                                                    type_detection>(*this);
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

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            bool CheckCache = true, typename IdType = none_t,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve_runtime_only(runtime_context &context, IdType &&id = IdType()) {
    using Type = normalized_type_t<T>;
    static_assert(!std::is_const_v<Type>);

    selected_runtime_binding<T, CheckCache, IdType> selected{runtime_registry_,
                                                             id};
    missing_runtime_binding<T, RemoveRvalueReferences, MayAutoConstruct, IdType>
        missing{*this, id};
    auto sources = detail::make_selected_binding_sources(selected, missing);
    return detail::resolve_from_binding_sources<T, R>(context, sources);
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_result_t<T, RemoveRvalueReferences>>
  R resolve_parent(none_t = {}) {
    if constexpr (std::is_void_v<Key>) {
      return parent_->template resolve<T>();
    } else {
      return parent_->template resolve<T>(key<Key>{});
    }
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache,
            typename Key = void,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve_parent(runtime_context &context) {
    using selector_type =
        std::conditional_t<std::is_void_v<Key>, void, key<Key>>;
    if constexpr (detail::is_context_resolve_supported_v<
                      parent_container_type, runtime_context, T,
                      RemoveRvalueReferences, CheckCache, selector_type>) {
      if constexpr (std::is_void_v<Key>) {
        return parent_->template resolve<T, RemoveRvalueReferences, CheckCache>(
            context);
      } else {
        return parent_->template resolve<T, RemoveRvalueReferences, CheckCache>(
            context, key<Key>{});
      }
    } else {
      return resolve_parent<T, RemoveRvalueReferences, Key>();
    }
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache,
            typename IdType,
            typename R = resolve_request_t<T, RemoveRvalueReferences>,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  R resolve_parent(runtime_context &context, IdType &&id) {
    if constexpr (detail::is_context_resolve_supported_v<
                      parent_container_type, runtime_context, T,
                      RemoveRvalueReferences, CheckCache, IdType &&>) {
      return parent_->template resolve<T, RemoveRvalueReferences, CheckCache>(
          context, std::forward<IdType>(id));
    } else {
      return parent_->template resolve<T>(std::forward<IdType>(id));
    }
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T>>
  R construct_runtime_only(Factory factory = Factory()) {
    runtime_context context;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      if (runtime_registry_.template binding_status<T>() !=
          detail::binding_selection_status::not_found) {
        if constexpr (!construct_normalized_request_v<T> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          return resolve_runtime_only<T, false,
                                      runtime_auto_constructible_v<T>>(
              context, none_t{});
        } else {
          return ::dingo::construct_request_or_wrap_normalized<T>(
              [&]() {
                return resolve_runtime_only<T, false,
                                            runtime_auto_constructible_v<T>>(
                    context, none_t{});
              },
              [&]() {
                return resolve_runtime_only<
                    normalized_type_t<T>, false,
                    runtime_auto_constructible_v<normalized_type_t<T>>>(
                    context, none_t{});
              });
        }
      } else if (runtime_registry_
                     .template binding_status<normalized_type_t<T>>() !=
                 detail::binding_selection_status::not_found) {
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          ::dingo::throw_missing_rvalue_conversion<T>(true, context);
        } else if constexpr (construct_normalized_request_v<T>) {
          return type_traits<std::decay_t<T>>::make(
              resolve_runtime_only<
                  normalized_type_t<T>, false,
                  runtime_auto_constructible_v<normalized_type_t<T>>>(
                  context, none_t{}));
        } else {
          return resolve_runtime_only<T, false,
                                      runtime_auto_constructible_v<T>>(
              context, none_t{});
        }
      }
    }

    if constexpr (construct_factory_request_v<T>) {
      auto type_guard = context.template track_type<normalized_type_t<T>>();
      return factory.template construct<R>(context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             T>) {
      ::dingo::throw_missing_rvalue_conversion<T>(false, context);
    } else {
      return resolve_runtime_only<T, false, runtime_auto_constructible_v<T>>(
          context, none_t{});
    }
  }

public:
  using runtime_registry_type = runtime_base;
  using static_registry_type = static_registry_type_;
  using container_traits_type = typename runtime_base::container_traits_type;
  using allocator_type = typename runtime_base::allocator_type;
  using rtti_type = typename runtime_base::rtti_type;
  using static_source_type = static_registry_type;

  static_assert(static_source_type::valid,
                "container requires a valid compile-time bindings source");
  static_assert(detail::key_value_bindings_are_declared<
                    typename static_source_type::interface_bindings,
                    index_entries_>::value,
                "container fixed dingo::key<Key, Value> bindings require "
                "static_container with associative<Key, Interface, one> or "
                "associative<Key, Interface, many>");
  static_assert(detail::key_value_bindings_are_unique<
                    typename static_source_type::interface_bindings,
                    index_entries_>::value,
                "container fixed runtime-key lookup bindings must be unique "
                "for one lookups and unique by storage for many lookups");
  static_assert(detail::graph_analysis<static_source_type, true>::resolvable,
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

  container_with_static_bindings() : runtime_registry_(nullptr) {}

  explicit container_with_static_bindings(allocator_type alloc)
      : runtime_registry_(nullptr, alloc) {}

  template <typename Parent = ParentContainer,
            std::enable_if_t<!std::is_void_v<Parent>, int> = 0>
  explicit container_with_static_bindings(
      Parent *parent, allocator_type alloc = allocator_type())
      : runtime_registry_(nullptr, alloc), parent_(parent) {}

  container_with_static_bindings(const self_type &other)
      : runtime_registry_(other.runtime_registry_),
        static_registry_(other.static_registry_),
        static_state_(other.static_state_), parent_(other.parent_) {}

  container_with_static_bindings(self_type &&other) noexcept
      : runtime_registry_(std::move(other.runtime_registry_)),
        static_registry_(std::move(other.static_registry_)),
        static_state_(std::move(other.static_state_)), parent_(other.parent_) {}

  self_type &operator=(const self_type &other) {
    if (this != &other) {
      runtime_registry_ = other.runtime_registry_;
      static_registry_ = other.static_registry_;
      static_state_ = other.static_state_;
      parent_ = other.parent_;
    }
    return *this;
  }

  self_type &operator=(self_type &&other) noexcept {
    if (this != &other) {
      runtime_registry_ = std::move(other.runtime_registry_);
      static_registry_ = std::move(other.static_registry_);
      static_state_ = std::move(other.static_state_);
      parent_ = other.parent_;
    }
    return *this;
  }

  self_type &registry() { return *this; }

  const self_type &registry() const { return *this; }

  self_type &container() { return *this; }

  const self_type &container() const { return *this; }

  template <typename T, typename IdType = none_t,
            typename R = request_result_t<T>>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    if constexpr (detail::is_typed_key_v<IdType>) {
      using key_type = typename std::decay_t<IdType>::type;
      if constexpr (collection_traits<R>::is_collection) {
        if constexpr (has_static_collection_v<R, key_type>) {
          if (select_static_collection<R, key_type>()) {
            return resolve_static<T, false, key_type>();
          }
        }
        if constexpr (has_parent_v) {
          if (parent_ && count_collection<R, key_type>() == 0) {
            return resolve_parent<T, false, key_type>();
          }
        }
      } else {
        if (select_static_resolve<T, key_type>()) {
          return resolve_static<T, false, key_type>();
        }
        if constexpr (has_parent_v) {
          if constexpr (static_resolve_status_v<T, false, key_type> ==
                        detail::binding_selection_status::not_found) {
            if (parent_ && resolve_binding_status<T, false, key_type>() ==
                               detail::binding_selection_status::not_found) {
              return resolve_parent<T, false, key_type>();
            }
          }
        }
      }
      runtime_context context;
      return resolve<T, false, true, key_type>(context);
    } else if constexpr (!is_none_v<std::decay_t<IdType>>) {
      if constexpr (has_parent_v) {
        if constexpr (collection_traits<R>::is_collection) {
          if (parent_ &&
              runtime_registry_.template count_runtime_collection<R>(id) == 0) {
            return parent_->template resolve<T>(std::forward<IdType>(id));
          }
        } else {
          if (parent_ &&
              runtime_registry_.template binding_status_for_id<T>(id) ==
                  detail::binding_selection_status::not_found) {
            return parent_->template resolve<T>(std::forward<IdType>(id));
          }
        }
      }
      runtime_context context;
      return resolve_runtime_only<T, false, runtime_auto_constructible_v<T>>(
          context, std::forward<IdType>(id));
    } else {
      if constexpr (collection_traits<R>::is_collection) {
        if constexpr (has_static_collection_v<R>) {
          if (select_static_collection<R>()) {
            return resolve_static<T, false>();
          }
        }
        if constexpr (has_parent_v) {
          if (parent_ && count_collection<R>() == 0) {
            return resolve_parent<T, false>();
          }
        }
      } else {
        if (select_static_resolve<T>()) {
          return resolve_static<T, false>();
        }
        if constexpr (has_parent_v) {
          if constexpr (static_resolve_status_v<T, false> ==
                        detail::binding_selection_status::not_found) {
            if (parent_ && resolve_binding_status<T, false>() ==
                               detail::binding_selection_status::not_found) {
              return resolve_parent<T, false>();
            }
          }
        }
      }
      runtime_context context;
      return resolve<T, false>(context);
    }
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = request_result_t<T>>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    using request_type = request_interface_t<T>;
    using normalized_request_type = request_value_t<T>;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<T>) {
        constexpr bool has_static_normalized_binding =
            static_selection_t<normalized_request_type, void>::status !=
                detail::binding_selection_status::not_found &&
            detail::static_binding_resolvable_v<
                typename static_selection_t<normalized_request_type,
                                            void>::binding_type,
                static_registry_type_>;

        if constexpr (has_static_construct_request_v<T>) {
          if (binding_status<request_type>() !=
                  detail::binding_selection_status::not_found &&
              select_static_construct<T>()) {
            return construct_static<T>();
          }
        }

        if (has_runtime_binding<request_type>() ||
            has_runtime_binding<normalized_request_type>()) {
          return construct_runtime_only<T>(std::move(factory));
        }

        if constexpr (has_static_normalized_binding) {
          ::dingo::throw_missing_rvalue_conversion<T>(true);
        }

        return construct_runtime_only<T>(std::move(factory));
      } else if constexpr (has_static_construct_request_v<T>) {
        if (binding_status<request_type>() !=
                detail::binding_selection_status::not_found &&
            select_static_construct<T>()) {
          return construct_static<T>();
        }
      } else {
        return construct_runtime_only<T>(std::move(factory));
      }
    }
    runtime_context context;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      if (binding_status<T>() != detail::binding_selection_status::not_found) {
        if constexpr (construct_normalized_request_v<T>) {
          return ::dingo::construct_request_or_wrap_normalized<T>(
              [&]() { return resolve<T, false>(context); },
              [&]() { return resolve<normalized_type_t<T>, false>(context); });
        } else {
          return resolve<T, false>(context);
        }
      } else if (binding_status<normalized_type_t<T>>() !=
                 detail::binding_selection_status::not_found) {
        if constexpr (construct_normalized_request_v<T>) {
          return type_traits<std::decay_t<T>>::make(
              resolve<normalized_type_t<T>, false>(context));
        } else {
          return resolve<T, false>(context);
        }
      }
    }

    if constexpr (construct_factory_request_v<T>) {
      auto type_guard = context.template track_type<normalized_type_t<T>>();
      return factory.template construct<R>(context, *this);
    } else {
      return resolve<T, false>(context);
    }
  }

  template <typename T> T construct_collection() {
    if (select_static_collection_construct<T>()) {
      return construct_static_collection<T>(none_t{});
    }

    return construct_collection_runtime_context<T>(
        detail::binding_collection_append{}, none_t{});
  }

  template <typename T, typename Fn> T construct_collection(Fn &&fn) {
    if (select_static_collection_construct<T>()) {
      return construct_static_collection<T>(std::forward<Fn>(fn), none_t{});
    }

    return construct_collection_runtime_context<T>(std::forward<Fn>(fn),
                                                   none_t{});
  }

  template <typename T, typename Key> T construct_collection(key<Key>) {
    if (select_static_collection_construct<T, Key>()) {
      return construct_static_collection<T>(key<Key>{});
    }

    return construct_collection_runtime_context<T>(
        detail::binding_collection_append{}, key<Key>{});
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key<Key>) {
    if (select_static_collection_construct<T, Key>()) {
      return construct_static_collection<T>(std::forward<Fn>(fn), key<Key>{});
    }

    return construct_collection_runtime_context<T>(std::forward<Fn>(fn),
                                                   key<Key>{});
  }

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        detail::callable_dispatch_signature_t<Signature, callable_type>;

    runtime_context context;
    auto type_guard = context.template track_type<callable_type>();
    return detail::callable_invoke<dispatch_signature>::construct(
        std::forward<Callable>(callable), context, *this);
  }

  template <typename Request, typename Key = void>
  detail::binding_selection_status binding_status() {
    using request_type = request_interface_t<Request>;
    using static_selection = static_selection_t<request_type, Key>;
    const auto runtime_status =
        runtime_registry_.template binding_status<request_type, Key>();
    return detail::resolve_binding_status<static_selection::status>(
        runtime_status,
        detail::binding_resolution_policy::ambiguous_on_conflict);
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void>
  detail::binding_selection_status resolve_binding_status() {
    using request_type = resolve_request_t<T, RemoveRvalueReferences>;
    const auto runtime_status =
        runtime_registry_.template binding_status<request_type, Key>();
    return detail::resolve_binding_status<
        static_resolve_status_v<T, RemoveRvalueReferences, Key>>(
        runtime_status,
        detail::binding_resolution_policy::ambiguous_on_conflict);
  }

  template <typename T, typename Key = void, typename Fn>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn) {
    return append_collection_impl<T, Key>(results, context, context,
                                          std::forward<Fn>(fn));
  }

  template <typename T, typename Key = void, typename Fn, typename Context,
            std::enable_if_t<!std::is_same_v<std::remove_reference_t<Context>,
                                             runtime_context>,
                             int> = 0>
  std::size_t append_collection(T &results, Context &context, Fn &&fn) {
    runtime_context runtime_append_context;
    return append_collection_impl<T, Key>(results, runtime_append_context,
                                          context, std::forward<Fn>(fn));
  }

  template <typename T, typename Key = void, typename Fn, typename Context>
  std::size_t append_collection_impl(T &results,
                                     runtime_context &runtime_context,
                                     Context &static_context, Fn &&fn) {
    binding_resolution_ref static_state_ref(static_state_);
    return detail::append_binding_collection(
        results,
        [&](auto &collection, auto &&append) {
          return runtime_registry_.append_runtime_collection(
              collection, runtime_context,
              std::forward<decltype(append)>(append), collection_key<Key>());
        },
        [&](auto &collection, auto &&append) {
          return static_state_ref
              .template append_static_collection<T, Key, static_registry_type_>(
                  collection, *this, static_context,
                  std::forward<decltype(append)>(append));
        },
        std::forward<Fn>(fn));
  }

  template <typename T, typename Key = void> std::size_t count_collection() {
    return detail::count_binding_collection<T>(
        [&] {
          return runtime_registry_.template count_runtime_collection<T>(
              collection_key<Key>());
        },
        detail::static_collection_binding_count<static_registry_type_, T,
                                                Key>());
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve_request(runtime_context &context) {
    return resolve<T, RemoveRvalueReferences, true>(context,
                                                    collection_key<Key>());
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
            typename R = resolve_result_t<T, RemoveRvalueReferences>>
  R resolve(static_context_type &context, none_t) {
    if constexpr (!has_static_resolve_request_v<T, RemoveRvalueReferences> &&
                  has_parent_v) {
      if constexpr (static_resolve_status_v<T, RemoveRvalueReferences> ==
                    detail::binding_selection_status::not_found) {
        if (parent_) {
          return resolve_parent<T, RemoveRvalueReferences>();
        }
      }
    }
    return resolve_static<T, RemoveRvalueReferences>(context);
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
            typename Key = void,
            typename R = resolve_result_t<T, RemoveRvalueReferences>>
  R resolve(static_context_type &context) {
    if constexpr (!has_static_resolve_request_v<T, RemoveRvalueReferences,
                                                Key> &&
                  has_parent_v) {
      if constexpr (static_resolve_status_v<T, RemoveRvalueReferences, Key> ==
                    detail::binding_selection_status::not_found) {
        if (parent_) {
          return resolve_parent<T, RemoveRvalueReferences, Key>();
        }
      }
    }
    return resolve_static<T, RemoveRvalueReferences, Key>(context);
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache,
            typename Key,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve(static_context_type &context, key<Key>) {
    if constexpr (!has_static_resolve_request_v<T, RemoveRvalueReferences,
                                                Key> &&
                  has_parent_v) {
      if constexpr (static_resolve_status_v<T, RemoveRvalueReferences, Key> ==
                    detail::binding_selection_status::not_found) {
        if (parent_) {
          return resolve_parent<T, RemoveRvalueReferences, Key>();
        }
      }
    }
    return resolve_static<T, RemoveRvalueReferences, Key>(context);
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
            typename R = resolve_result_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, none_t) {
    return resolve<T, RemoveRvalueReferences, CheckCache>(context);
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache = true,
            typename Key = void,
            typename R = resolve_result_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v) {
        if (parent_ && count_collection<R, Key>() == 0) {
          return resolve_parent<T, RemoveRvalueReferences, CheckCache, Key>(
              context);
        }
      }
      return construct_collection<R>(
          context, detail::binding_collection_append{}, collection_key<Key>());
    } else {
      if constexpr (has_parent_v) {
        if constexpr (static_resolve_status_v<T, RemoveRvalueReferences, Key> ==
                      detail::binding_selection_status::not_found) {
          if (parent_ &&
              resolve_binding_status<T, RemoveRvalueReferences, Key>() ==
                  detail::binding_selection_status::not_found) {
            return resolve_parent<T, RemoveRvalueReferences, CheckCache, Key>(
                context);
          }
        }
      }
      using request_type = R;
      runtime_binding_candidate<T, RemoveRvalueReferences, Key, request_type>
          runtime{*this};
      static_binding_candidate<request_type, Key> static_binding{*this};
      auto sources = detail::make_two_binding_sources(
          runtime, static_binding, runtime,
          detail::binding_resolution_policy::ambiguous_on_conflict);
      return detail::resolve_from_binding_sources<T, request_type>(context,
                                                                   sources);
    }
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache,
            typename Key,
            typename R = resolve_request_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, key<Key>) {
    return resolve<T, RemoveRvalueReferences, CheckCache, Key>(context);
  }

  template <typename T, bool RemoveRvalueReferences, bool CheckCache,
            typename IdType,
            typename R = resolve_request_t<T, RemoveRvalueReferences>,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  R resolve(runtime_context &context, IdType &&id) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v) {
        if (parent_ &&
            runtime_registry_.template count_runtime_collection<R>(id) == 0) {
          return resolve_parent<T, RemoveRvalueReferences, CheckCache>(
              context, std::forward<IdType>(id));
        }
      }
    } else if constexpr (has_parent_v) {
      if (parent_ && runtime_registry_.template binding_status_for_id<T>(id) ==
                         detail::binding_selection_status::not_found) {
        return resolve_parent<T, RemoveRvalueReferences, CheckCache>(
            context, std::forward<IdType>(id));
      }
    }
    return resolve_runtime_only<T, RemoveRvalueReferences,
                                runtime_auto_constructible_v<T>, CheckCache>(
        context, std::forward<IdType>(id));
  }

private:
  friend class detail::runtime_registration_api<self_type>;

  runtime_registry_type &runtime_registry_ref() { return runtime_registry_; }

  self_type &runtime_registration_parent() { return *this; }

  runtime_registry_type runtime_registry_;
  static_registry_type static_registry_;
  static_state static_state_;
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
