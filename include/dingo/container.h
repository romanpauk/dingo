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
#include <dingo/static/context.h>
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
template <typename ParentContainer, typename... Registrations>
class detail::container_with_static_bindings<static_bindings<Registrations...>,
                                             ParentContainer>
    : public detail::runtime_registration_api<
          detail::container_with_static_bindings<
              static_bindings<Registrations...>, ParentContainer>> {
  friend class runtime_context;
  template <typename T, typename Context, typename Container>
  friend T detail::resolve_context_request(Context &, Container &);

public:
  using static_bindings_type = static_bindings<Registrations...>;
  using static_registry_type =
      detail::static_registry<static_bindings_type,
                              detail::static_storage_state<Registrations...>>;

private:
  using self_type = detail::container_with_static_bindings<static_bindings_type,
                                                           ParentContainer>;
  using runtime_base =
      runtime_registry<dynamic_container_traits,
                       typename dynamic_container_traits::allocator_type,
                       self_type, self_type>;
  friend runtime_base;

  using static_state = detail::static_storage_state<Registrations...>;
  using static_context_type = static_context<static_bindings_type>;
  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<dynamic_container_traits>>;
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using parent_container_type = ParentContainer;

  template <typename T, typename Key>
  using static_selection_t = detail::static_binding_t<
      typename static_bindings_type::template bindings<T, Key>>;

  template <typename T>
  static constexpr bool runtime_auto_constructible_v =
      std::is_same_v<dependency_value_t<T>, std::decay_t<T>> &&
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
    return has_runtime_binding<Request>(key) ||
           has_runtime_binding<NormalizedRequest>(key);
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_collection(LookupKey key) {
    return runtime_registry_.template count_collection<T>(key) != 0;
  }

  template <typename T, bool RemoveRvalueReferences, typename Key>
  static constexpr detail::binding_status static_resolve_status_v =
      static_selection_t<resolve_dependency_t<T, RemoveRvalueReferences>,
                         Key>::status;

  template <typename Collection, typename Key> bool select_static_collection() {
    if constexpr (!has_static_collection_v<Collection, Key>) {
      return false;
    } else {
      return !has_runtime_collection<Collection>(Key{});
    }
  }

  template <typename T>
  static constexpr bool has_static_construct_v = [] {
    using request_type = dependency_interface_t<T>;
    using normalized_request_type = dependency_value_t<T>;
    using no_key = detail::no_lookup_key_t;
    constexpr bool has_exact_static_binding =
        static_registry_type::template is_binding_resolvable<request_type,
                                                             no_key>();
    constexpr bool has_normalized_static_binding =
        static_registry_type::template is_binding_resolvable<
            normalized_request_type, no_key>();

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

  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename R = dependency_result_t<T>,
            std::enable_if_t<std::is_rvalue_reference_v<T>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve_static() {
    static_context_type static_context;
    return resolve_static<T, RemoveRvalueReferences, Key>(static_context);
  }

  template <typename T, bool RemoveRvalueReferences, typename Key,
            std::enable_if_t<!std::is_rvalue_reference_v<T>, int> = 0>
  DINGO_ALWAYS_INLINE decltype(auto) resolve_static() {
    static_context_type static_context;
    return resolve_static<T, RemoveRvalueReferences, Key>(static_context);
  }

  template <typename T, typename R = dependency_result_t<T>>
  DINGO_ALWAYS_INLINE R construct_static() {
    using request_type = dependency_interface_t<T>;
    using normalized_request_type = dependency_value_t<T>;
    using no_key = detail::no_lookup_key_t;
    constexpr bool has_exact_static_binding =
        static_registry_type::template is_binding_resolvable<request_type,
                                                             no_key>();
    constexpr bool has_normalized_static_binding =
        static_registry_type::template is_binding_resolvable<
            normalized_request_type, no_key>();
    if constexpr (has_exact_static_binding) {
      using binding =
          typename static_selection_t<request_type, no_key>::binding_type;
      if constexpr (detail::binding_supports_request_v<T, binding>) {
        static_context_type static_context;
        return resolve_static<T, false, no_key>(static_context);
      }
    }

    if constexpr (has_normalized_static_binding &&
                  construct_normalized_request_v<T>) {
      using normalized_selection =
          static_selection_t<normalized_request_type, no_key>;
      return detail::construct_static_binding_value<T, normalized_selection>(
          [&]() {
            return resolve_static<normalized_request_type, false, no_key>();
          });
    } else {
      static_context_type static_context;
      return resolve_static<T, false, no_key>(static_context);
    }
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_impl(runtime_context &context, LookupKey key,
                              Fn &&fn) {
    return detail::construct_binding_collection<T>(
        [&] { return runtime_registry_.template count_collection<T>(key); },
        [&] {
          return static_registry_.template count_collection<T, LookupKey>();
        },
        [&](auto &results, auto &&append) {
          runtime_registry_.template append_collection<T>(
              results, context, std::forward<decltype(append)>(append), key);
        },
        [&](auto &results, auto &&append) {
          static_registry_.template append_collection<T, LookupKey>(
              results, *this, context, std::forward<decltype(append)>(append));
        },
        std::forward<Fn>(fn));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(runtime_context &context, Fn &&fn, LookupKey key) {
    return construct_collection_impl<T>(context, std::move(key),
                                        std::forward<Fn>(fn));
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename Context,
            typename R = resolve_dependency_result_t<T, RemoveRvalueReferences>>
  DINGO_ALWAYS_INLINE R resolve_static(Context &context) {
    return static_registry_
        .template resolve_request<T, RemoveRvalueReferences, LookupKey>(context,
                                                                        *this);
  }

  template <typename T, typename Fn, typename LookupKey>
#if defined(__GNUC__) && !defined(__clang__)
  __attribute__((noinline))
#endif
  T construct_collection_runtime_context(Fn &&fn, LookupKey key) {
    runtime_context context;
    return construct_collection<T>(context, std::forward<Fn>(fn),
                                   std::move(key));
  }

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename LookupKey,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_missing(runtime_context &context, LookupKey key) {
    using Type = normalized_type_t<T>;

    if constexpr (MayAutoConstruct &&
                  detail::is_static_lookup_key_definition_v<LookupKey> &&
                  !detail::is_no_lookup_key_v<LookupKey> &&
                  collection_traits<R>::is_collection) {
      return construct_collection_runtime_context<R>(
          detail::binding_collection_append{}, std::move(key));
    } else if constexpr (MayAutoConstruct &&
                         is_auto_constructible<std::decay_t<T>>::value) {
      if constexpr (constructor<Type>::kind ==
                    detail::constructor_kind::concrete) {
        static_assert(is_complete<Type>::value,
                      "auto-construction requires a complete type");
        using type_detection = detail::automatic;
        return context.template construct_temporary<dependency_interface_t<T>,
                                                    type_detection>(*this);
      } else {
        throw detail::make_type_not_found_exception<T>(context, key);
      }
    } else {
      throw detail::make_type_not_found_exception<T>(context, key);
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename R,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_missing_request(runtime_context &context, LookupKey id) {
    if constexpr (has_parent_v) {
      if (parent_) {
        if (!detail::should_resolve_missing_from_parent<T>(*parent_, id)) {
          return resolve_missing<T, RemoveRvalueReferences,
                                 runtime_auto_constructible_v<T>, decltype(id),
                                 R>(context, std::move(id));
        }
        return resolve_parent<T, RemoveRvalueReferences>(context, id);
      }
    }
    return resolve_missing<T, RemoveRvalueReferences,
                           runtime_auto_constructible_v<T>, decltype(id), R>(
        context, std::move(id));
  }

  template <typename T, typename LookupKey,
            typename R = resolve_dependency_result_t<T, false>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(LookupKey key) {
    return parent_->template resolve<T>(key);
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(runtime_context &context, LookupKey key) {
    if constexpr (detail::is_context_resolve_supported_v<
                      parent_container_type, runtime_context, T,
                      RemoveRvalueReferences, LookupKey>) {
      return parent_->template resolve<T, RemoveRvalueReferences>(context, key);
    } else {
      return resolve_parent<T>(key);
    }
  }

  template <typename T, typename R = resolve_dependency_t<T, false>>
  R resolve_runtime_request(runtime_context &context) {
    auto key = detail::no_lookup_key();
    auto selection = runtime_registry_.template select_binding<T>(key);
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<T>(context);
    }
    if (selection.status == detail::binding_status::found) {
      using request_type = dependency_interface_t<T>;
      return runtime_registry_.template resolve_binding<request_type, R>(
          selection, context);
    }
    return resolve_missing<T, false, runtime_auto_constructible_v<T>,
                           decltype(key), R>(context, std::move(key));
  }

  template <typename T, typename R = dependency_result_t<T>>
  R construct_runtime_resolved_request(runtime_context &context) {
    try {
      return resolve_runtime_request<T>(context);
    } catch (const type_not_convertible_exception &) {
      auto &&value = resolve_runtime_request<normalized_type_t<T>>(context);
      return type_traits<std::decay_t<T>>::make(
          std::forward<decltype(value)>(value));
    }
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = dependency_result_t<T>>
  R construct_runtime_request(Factory factory = Factory()) {
    runtime_context context;

    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      auto key = detail::no_lookup_key();
      if (runtime_registry_.template binding_status<T>(key) !=
          detail::binding_status::not_found) {
        if constexpr (!construct_normalized_request_v<T> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          return resolve_runtime_request<T>(context);
        } else {
          return construct_runtime_resolved_request<T, R>(context);
        }
      } else if (runtime_registry_
                     .template binding_status<normalized_type_t<T>>(key) !=
                 detail::binding_status::not_found) {
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          ::dingo::throw_missing_rvalue_conversion<T>(true, context);
        } else if constexpr (construct_normalized_request_v<T>) {
          return type_traits<std::decay_t<T>>::make(
              resolve_runtime_request<normalized_type_t<T>>(context));
        } else {
          return resolve_runtime_request<T>(context);
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
      return resolve_runtime_request<T>(context);
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
  static_assert(detail::graph_analysis<static_bindings_type, true>::resolvable,
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
  template <typename T, typename R, typename LookupKey>
  DINGO_ALWAYS_INLINE R resolve_static_lookup_key_request(LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_static_collection_v<R, LookupKey>) {
        if (select_static_collection<R, LookupKey>()) {
          return resolve_static<T, false, LookupKey>();
        }
      }
      if constexpr (has_parent_v) {
        if (parent_ && count_collection<R>(key) == 0) {
          return resolve_parent<T>(key);
        }
      }
    } else {
      if constexpr (static_registry_type::template is_binding_resolvable<
                        resolve_dependency_t<T, false>, LookupKey>()) {
        if (!has_runtime_binding<T>(key)) {
          return resolve_static<T, false, LookupKey>();
        }
      }
    }
    runtime_context context;
    return resolve<T, false>(context, std::move(key));
  }

  template <typename T, typename R, typename LookupKey>
  DINGO_ALWAYS_INLINE R resolve_runtime_lookup_key_request(LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v) {
        if (parent_ &&
            runtime_registry_.template count_collection<R>(key) == 0) {
          return resolve_parent<T>(key);
        }
      }
    } else {
      if constexpr (has_parent_v) {
        if (parent_ && runtime_registry_.template binding_status<T>(key) ==
                           detail::binding_status::not_found) {
          return resolve_parent<T>(key);
        }
      }
    }

    runtime_context context;
    auto selection = runtime_registry_.template select_binding<T>(key);
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<T>(context);
    }
    if (selection.status == detail::binding_status::found) {
      using request_type = dependency_interface_t<T>;
      return runtime_registry_.template resolve_binding<request_type, R>(
          selection, context);
    }
    return resolve_missing<T, false, runtime_auto_constructible_v<T>, LookupKey,
                           R>(context, std::move(key));
  }

  template <typename T, typename R, typename LookupKey>
  DINGO_ALWAYS_INLINE R resolve_lookup_key_request(LookupKey key) {
    if constexpr (detail::is_static_lookup_key_definition_v<LookupKey>) {
      return resolve_static_lookup_key_request<T, R>(std::move(key));
    } else {
      return resolve_runtime_lookup_key_request<T, R>(std::move(key));
    }
  }

public:
  template <typename T, typename IdType = none_t,
            typename R = dependency_result_t<T>,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return resolve_lookup_key_request<T, R>(std::move(key));
  }

  template <typename T, typename LookupKey, typename R = dependency_result_t<T>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    return resolve_lookup_key_request<T, R>(std::move(key));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = dependency_result_t<T>>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    using request_type = dependency_interface_t<T>;
    using normalized_request_type = dependency_value_t<T>;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      const bool has_runtime_no_key =
          has_runtime_no_key_binding<request_type, normalized_request_type>();
      if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<T>) {
        using no_key = detail::no_lookup_key_t;
        constexpr bool has_static_normalized_binding =
            static_selection_t<normalized_request_type, no_key>::status !=
                detail::binding_status::not_found &&
            detail::static_binding_resolvable_v<
                typename static_selection_t<normalized_request_type,
                                            no_key>::binding_type,
                static_bindings_type>;

        if constexpr (has_static_construct_v<T>) {
          if (!has_runtime_no_key) {
            return construct_static<T>();
          }
        }

        if (has_runtime_no_key) {
          return construct_runtime_request<T>(std::move(factory));
        }

        if constexpr (has_static_normalized_binding) {
          ::dingo::throw_missing_rvalue_conversion<T>(true);
        }

        return construct_runtime_request<T>(std::move(factory));
      } else if constexpr (has_static_construct_v<T>) {
        if (!has_runtime_no_key) {
          return construct_static<T>();
        }
      } else {
        return construct_runtime_request<T>(std::move(factory));
      }
    }
    runtime_context context;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      auto key = detail::no_lookup_key();
      const auto status = binding_status<T>(key);
      if (status != detail::binding_status::not_found) {
        if constexpr (construct_normalized_request_v<T>) {
          return ::dingo::construct_resolved_request<T>(
              [&]() { return resolve<T, false>(context, key); },
              [&]() {
                return resolve<normalized_type_t<T>, false>(context, key);
              });
        } else {
          return resolve<T, false>(context, key);
        }
      } else if (binding_status<normalized_type_t<T>>(key) !=
                 detail::binding_status::not_found) {
        if constexpr (construct_normalized_request_v<T>) {
          return type_traits<std::decay_t<T>>::make(
              resolve<normalized_type_t<T>, false>(context, key));
        } else {
          return resolve<T, false>(context, key);
        }
      }
    }

    if constexpr (construct_factory_request_v<T>) {
      auto type_guard = context.template track_type<normalized_type_t<T>>();
      return factory.template construct<R>(context, *this);
    } else {
      return resolve<T, false>(context, detail::no_lookup_key());
    }
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
        static_context_type static_context;
        return static_registry_.template construct_collection<T, LookupKey>(
            *this, static_context);
      }
    }

    return construct_collection_runtime_context<T>(
        detail::binding_collection_append{}, std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      if (select_static_collection_construct<T, LookupKey>()) {
        static_context_type static_context;
        return static_registry_.template construct_collection<T, LookupKey>(
            *this, static_context, std::forward<Fn>(fn));
      }
    }

    return construct_collection_runtime_context<T>(std::forward<Fn>(fn),
                                                   std::move(key));
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

    runtime_context context;
    auto type_guard = context.template track_type<callable_type>();
    return detail::callable_invoke<dispatch_signature>::construct(
        std::forward<Callable>(callable), context, *this);
  }

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(LookupKey key) {
    using request_type = dependency_interface_t<Request>;
    using static_selection = static_selection_t<request_type, LookupKey>;
    const auto runtime_status =
        runtime_registry_.template binding_status<request_type>(key);
    return detail::resolve_binding_status<static_selection::status>(
        runtime_status,
        detail::binding_resolution_policy::ambiguous_on_conflict);
  }

  template <typename T, typename Fn>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn) {
    return append_collection<T>(results, context, std::forward<Fn>(fn),
                                detail::no_lookup_key());
  }

  template <typename T, typename Key, typename Fn>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn) {
    return append_collection<T>(results, context, std::forward<Fn>(fn),
                                detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename Fn, typename Context,
            std::enable_if_t<!std::is_same_v<std::remove_reference_t<Context>,
                                             runtime_context>,
                             int> = 0>
  std::size_t append_collection(T &results, Context &context, Fn &&fn) {
    return append_collection<T>(results, context, std::forward<Fn>(fn),
                                detail::no_lookup_key());
  }

  template <typename T, typename Key, typename Fn, typename Context,
            std::enable_if_t<!std::is_same_v<std::remove_reference_t<Context>,
                                             runtime_context>,
                             int> = 0>
  std::size_t append_collection(T &results, Context &context, Fn &&fn) {
    return append_collection<T>(results, context, std::forward<Fn>(fn),
                                detail::make_lookup_key(type_selector<Key>{}));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn,
                                LookupKey key) {
    return append_collection_impl<T>(results, context, context, std::move(key),
                                     std::forward<Fn>(fn));
  }

  template <typename T, typename Fn, typename Context, typename LookupKey,
            std::enable_if_t<!std::is_same_v<std::remove_reference_t<Context>,
                                             runtime_context> &&
                                 detail::is_lookup_key_v<LookupKey>,
                             int> = 0>
  std::size_t append_collection(T &results, Context &context, Fn &&fn,
                                LookupKey key) {
    runtime_context runtime_append_context;
    return append_collection_impl<T>(results, runtime_append_context, context,
                                     std::move(key), std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename Fn, typename Context,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t
  append_collection_impl(T &results, runtime_context &runtime_context,
                         Context &static_context, LookupKey key, Fn &&fn) {
    return detail::append_binding_collection(
        results,
        [&](auto &collection, auto &&append) {
          return runtime_registry_.template append_collection<T>(
              collection, runtime_context,
              std::forward<decltype(append)>(append), key);
        },
        [&](auto &collection, auto &&append) {
          return static_registry_.template append_collection<T, LookupKey>(
              collection, *this, static_context,
              std::forward<decltype(append)>(append));
        },
        std::forward<Fn>(fn));
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
    return detail::count_binding_collection<T>(
        [&] { return runtime_registry_.template count_collection<T>(key); },
        static_registry_.template count_collection<T, LookupKey>());
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(static_context_type &context, LookupKey key) {
    constexpr auto status =
        static_resolve_status_v<T, RemoveRvalueReferences, LookupKey>;
    if constexpr (status == detail::binding_status::found) {
      return resolve_static<T, RemoveRvalueReferences, LookupKey>(context);
    } else if constexpr (status == detail::binding_status::ambiguous) {
      static_assert(status != detail::binding_status::ambiguous,
                    "container cannot resolve an ambiguously bound static "
                    "type");
    } else {
      if constexpr (has_parent_v) {
        if (parent_) {
          return resolve_parent<T>(std::move(key));
        }
      }
      static_assert(has_parent_v, "container cannot resolve an unbound static "
                                  "type");
      using lookup_request_type =
          resolve_dependency_t<T, RemoveRvalueReferences>;
      throw detail::make_type_not_found_exception<lookup_request_type>();
    }
  }

private:
  template <typename T, bool RemoveRvalueReferences, typename R,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_singular(runtime_context &context, LookupKey key) {
    using lookup_request_type = resolve_dependency_t<T, RemoveRvalueReferences>;
    using request_type = R;
    auto runtime_selection =
        runtime_registry_.template select_binding<lookup_request_type>(key);
    using static_selection =
        typename static_registry_type::template selection<lookup_request_type,
                                                          LookupKey>;
    if (runtime_selection.status == detail::binding_status::ambiguous ||
        static_selection::status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<T>(context);
    }
    if constexpr (static_selection::status == detail::binding_status::found) {
      if (runtime_selection.status == detail::binding_status::found) {
        throw detail::make_type_ambiguous_exception<T>(context);
      }
      return static_registry_.template resolve_binding<
          lookup_request_type, request_type, static_selection>(context, *this);
    } else {
      if (runtime_selection.status == detail::binding_status::found) {
        return runtime_registry_
            .template resolve_binding<request_type, request_type>(
                runtime_selection, context);
      }
      if constexpr (has_parent_v) {
        if (parent_) {
          return resolve_parent<T, RemoveRvalueReferences>(context, key);
        }
      }
      return resolve_missing_request<T, RemoveRvalueReferences, R>(context,
                                                                   key);
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename R,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_static_lookup_key_request(runtime_context &context, LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v) {
        if (parent_ && count_collection<R>(key) == 0) {
          return resolve_parent<T, RemoveRvalueReferences>(context, key);
        }
      }
      return construct_collection<R>(
          context, detail::binding_collection_append{}, std::move(key));
    } else {
      return resolve_singular<T, RemoveRvalueReferences, R>(context,
                                                            std::move(key));
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename R,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_runtime_lookup_key_request(runtime_context &context,
                                       LookupKey key) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (has_parent_v) {
        if (parent_ &&
            runtime_registry_.template count_collection<R>(key) == 0) {
          return resolve_parent<T, RemoveRvalueReferences>(context, key);
        }
      }
    } else {
      if constexpr (has_parent_v) {
        if (parent_ && runtime_registry_.template binding_status<T>(key) ==
                           detail::binding_status::not_found) {
          return resolve_parent<T, RemoveRvalueReferences>(context, key);
        }
      }
    }
    auto selection = runtime_registry_.template select_binding<T>(key);
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<T>(context);
    }
    if (selection.status == detail::binding_status::found) {
      using request_type = dependency_interface_t<T>;
      return runtime_registry_.template resolve_binding<request_type, R>(
          selection, context);
    }
    return resolve_missing<T, RemoveRvalueReferences,
                           runtime_auto_constructible_v<T>, LookupKey, R>(
        context, std::move(key));
  }

public:
  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(runtime_context &context, LookupKey key) {
    if constexpr (detail::is_static_lookup_key_definition_v<LookupKey>) {
      return resolve_static_lookup_key_request<T, RemoveRvalueReferences, R>(
          context, std::move(key));
    } else {
      return resolve_runtime_lookup_key_request<T, RemoveRvalueReferences, R>(
          context, std::move(key));
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
