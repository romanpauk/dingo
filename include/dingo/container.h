//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/complete_type.h>
#include <dingo/base/exceptions.h>
#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
#include <dingo/binding/binding_resolution.h>
#include <dingo/binding/binding_selection.h>
#include <dingo/container_base.h>
#include <dingo/factory/callable.h>
#include <dingo/factory/constructor.h>
#include <dingo/factory/invoke.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/lookup/lookup.h>
#include <dingo/memory/allocator.h>
#include <dingo/registration/requirements.h>
#include <dingo/registration/type_registration.h>
#include <dingo/rtti/static_provider.h>
#include <dingo/rtti/typeid_provider.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/context.h>
#include <dingo/runtime/registration_api.h>
#include <dingo/runtime/registry.h>
#include <dingo/runtime/runtime_binding.h>
#include <dingo/static/container_traits.h>
#include <dingo/storage/interface_storage_traits.h>
#include <dingo/type/annotated.h>
#include <dingo/type/auto_constructible.h>
#include <dingo/type/collection_traits.h>
#include <dingo/type/dependency.h>
#include <dingo/type/dependency_traits.h>
#include <dingo/type/key.h>
#include <dingo/type/normalized_type.h>

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

// The resolve front-end of a runtime container. It owns the root runtime
// registry and answers the requests the registry does not: parent fallback,
// auto-construction and the error paths.
template <typename ContainerTraits, typename Allocator,
          typename ParentContainer>
class detail::runtime_container_impl
    : public detail::runtime_registration_api<detail::runtime_container_impl<
          ContainerTraits, Allocator, ParentContainer>> {
  template <typename> friend class runtime_context;

  using self_type = detail::runtime_container_impl<ContainerTraits, Allocator,
                                                   ParentContainer>;
  using runtime_base =
      runtime_registry<ContainerTraits, Allocator, void, self_type>;
  using runtime_context_type =
      runtime_context<typename runtime_base::allocator_type>;
  friend runtime_base;
  template <typename, typename, typename, typename, bool>
  friend class ::dingo::runtime_registry;

public:
  // A container without an explicit parent type accepts a parent of its own
  // type.
  using parent_container_type =
      std::conditional_t<std::is_void_v<ParentContainer>, self_type,
                         detail::container_implementation_t<ParentContainer>>;
  using runtime_registry_type = runtime_base;
  using registry_type = runtime_base;
  using container_type = self_type;
  using container_traits_type = typename runtime_base::container_traits_type;
  using allocator_type = typename runtime_base::allocator_type;
  using rtti_type = typename runtime_base::rtti_type;
  using lookup_definition_type = typename runtime_base::lookup_definition_type;

private:
  template <typename T>
  static constexpr bool runtime_auto_constructible_v =
      detail::is_runtime_auto_constructible_dependency_v<T>;

  // The parent answers only the lookups it declares.
  template <typename Lookup, typename LookupKey>
  static constexpr bool parent_serves() {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return detail::has_lookup<parent_container_type, Lookup, LookupKey>();
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(construction_scope scope,
                         runtime_context_type &context, Fn &&fn,
                         LookupKey key) {
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
    runtime_registry_.template append_collection<T>(scope, results, context, fn,
                                                    std::move(key));
    return results;
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
  runtime_container_impl()
      : runtime_registry_(detail::runtime_data_owner, this) {}

  explicit runtime_container_impl(allocator_type alloc)
      : runtime_registry_(detail::runtime_data_owner, this, alloc) {}

  runtime_container_impl(parent_container_type *parent,
                         const allocator_type &alloc = allocator_type())
      : runtime_registry_(detail::runtime_data_owner, this, alloc),
        parent_(parent) {}

  // Resolved instances reference each other by address and runtime bindings
  // point back at the container, so it cannot move. runtime_container moves by
  // owning it through a handle.
  runtime_container_impl(const self_type &) = delete;
  runtime_container_impl(self_type &&) = delete;
  self_type &operator=(const self_type &) = delete;
  self_type &operator=(self_type &&) = delete;

  self_type &container() { return *this; }

  const self_type &container() const { return *this; }

private:
  template <typename Request, typename R> static constexpr bool is_cacheable() {
    return detail::cache::supports_v<typename Request::interface_type> &&
           !collection_traits<R>::is_collection;
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
    if constexpr (is_cacheable<Request, R>()) {
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
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context_type &context) -> R {
          return construct_request<request_type<T>, Factory, R>(
              ephemeral_scope, context, std::move(factory));
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
    return construct_collection<T>(detail::binding_collection_append{},
                                   std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
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

  template <typename T, typename Fn> T construct_collection(Fn &&fn, none_t) {
    return construct_collection<T>(std::forward<Fn>(fn),
                                   detail::no_lookup_key());
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
    auto status = runtime_registry_.template binding_status<
        typename request::value_type, typename request::exact_type>(key);
    if constexpr (parent_serves<typename request::lookup_type,
                                decltype(key)>()) {
      if (parent_ && status == detail::binding_status::not_found) {
        return parent_->template binding_status<Request>(key);
      }
    }
    return status;
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(construction_scope scope, T &results,
                                runtime_context_type &context, Fn &&fn,
                                LookupKey key) {
    return runtime_registry_.template append_collection<T>(
        scope, results, context, std::forward<Fn>(fn), std::move(key));
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
    return runtime_registry_.template count_collection<T>(key);
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

  template <typename Request, typename R, typename Origin, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_collection(construction_scope scope, runtime_context_type &context,
                       Origin &origin, LookupKey key) {
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
    if (count_collection<R>(key) == 0) {
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
      return construct_collection<R>(
          scope, context, detail::binding_collection_append{}, std::move(key));
    } else {
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
      return resolve_collection<Request, R>(scope, context, origin,
                                            std::move(key));
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
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R unresolved(construction_scope scope, runtime_context_type &context,
               LookupKey key) {
    using Type = typename Request::value_type;
    using user_type = typename Request::user_type;

    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (detail::default_collection_append_v<R>) {
        return construct_collection<R>(scope, context,
                                       detail::binding_collection_append{},
                                       std::move(key));
      } else {
        throw detail::make_type_not_found_exception<
            typename Request::lookup_type>(context, key);
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
class container : public runtime_container<Params...> {
public:
  using runtime_container<Params...>::runtime_container;
};
} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
