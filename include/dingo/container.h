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

  using index_entries_ = detail::normalize_lookup_definitions_t<
      detail::container_lookup_definition_type_t<dynamic_container_traits>>;
  static constexpr bool has_parent_v = !std::is_void_v<ParentContainer>;
  using parent_container_type = ParentContainer;

  template <typename T, typename Key>
  using static_selection_t = detail::static_binding_t<
      typename static_bindings_type::template bindings<T, Key>>;

  template <typename T>
  static constexpr bool runtime_auto_constructible_v =
      std::is_same_v<typename request_type<T>::value_type, std::decay_t<T>> &&
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
    static_assert(
        std::is_same_v<typename request_type<Request>::value_type,
                       typename request_type<NormalizedRequest>::value_type>);
    return has_runtime_binding<Request>(key);
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  bool has_runtime_collection(LookupKey key) {
    return runtime_registry_.template count_collection<T>(key) != 0;
  }

  template <typename Request, typename Key>
  static constexpr detail::binding_status static_resolve_status_v =
      static_selection_t<typename Request::lookup_type, Key>::status;

  template <typename Request, typename Key,
            bool Found = static_selection_t<Request, Key>::status ==
                         detail::binding_status::found>
  struct is_static_binding_available : std::false_type {};

  template <typename Request, typename Key>
  struct is_static_binding_available<Request, Key, true> {
    using binding = typename static_selection_t<Request, Key>::binding_type;
    static constexpr bool value = detail::binding_supports_request_v<
        typename request_type<Request>::interface_type, binding>;
  };

  template <typename Request, typename Key>
  static constexpr bool is_static_binding_available_v =
      is_static_binding_available<Request, Key>::value;
  template <typename T>
  static constexpr bool has_static_construct_v = [] {
    using request = request_type<T>;
    using interface_type = typename request::interface_type;
    using value_type = typename request::value_type;
    using no_key = detail::no_lookup_key_t;
    constexpr bool has_exact_static_binding =
        is_static_binding_available_v<interface_type, no_key>;
    constexpr bool has_normalized_static_binding =
        is_static_binding_available_v<value_type, no_key>;

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

  template <typename Request, typename R = typename Request::result_type>
  DINGO_ALWAYS_INLINE R construct_static(runtime_context &context) {
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
        return resolve_static<Request, no_key>(context);
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
                context);
          });
    } else {
      return resolve_static<Request, no_key>(context);
    }
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection_impl(runtime_context &context, LookupKey key,
                              Fn &&fn) {
    using collection_type = collection_traits<T>;
    using resolve_type = typename collection_type::resolve_type;

    static_assert(collection_type::is_collection,
                  "missing collection_traits specialization for type T");

    const std::size_t runtime_count =
        runtime_registry_.template count_collection<T>(key);
    const std::size_t static_count =
        static_registry_.template count_collection<T, LookupKey>();
    const std::size_t total = runtime_count + static_count;
    // A declared collection lookup with no rows is a valid empty collection.
    if (total == 0 &&
        !runtime_registry_.template has_explicit_collection_lookup<T>(key)) {
      throw detail::make_collection_type_not_found_exception<T, resolve_type>();
    }

    T results;
    collection_type::reserve(results, total);

    auto &&append = fn;
    runtime_registry_.template append_collection<T>(results, context, append,
                                                    key);
    static_registry_.template append_collection<T, LookupKey>(results, *this,
                                                              context, append);
    return results;
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(runtime_context &context, Fn &&fn, LookupKey key) {
    return construct_collection_impl<T>(context, std::move(key),
                                        std::forward<Fn>(fn));
  }

  template <typename Request, typename LookupKey, typename Context,
            typename R = typename Request::result_type>
  DINGO_ALWAYS_INLINE R resolve_static(Context &context) {
    return static_registry_.template resolve_request<Request, LookupKey>(
        context, *this);
  }

  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_missing(runtime_context &context, LookupKey key) {
    using Type = typename Request::value_type;
    using user_type = typename Request::user_type;

    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (detail::default_collection_append_v<R>) {
        return construct_collection<R>(
            context, detail::binding_collection_append{}, std::move(key));
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
        using type_detection = detail::automatic;
        return context.template construct<typename Request::interface_type,
                                          type_detection>(*this);
      } else {
        throw detail::make_type_not_found_exception<
            typename Request::lookup_type>(context, key);
      }
    } else {
      throw detail::make_type_not_found_exception<
          typename Request::lookup_type>(context, key);
    }
  }

  template <typename Request, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(runtime_context &context, LookupKey key) {
    using user_type = typename Request::user_type;
    return parent_
        ->template resolve<user_type, Request::removes_rvalue_references>(
            context, key);
  }

  template <typename Request, typename R = typename Request::lookup_type>
  R resolve_request(runtime_context &context) {
    auto key = detail::no_lookup_key();
    auto selection =
        runtime_registry_
            .template select_binding<typename Request::lookup_type>(key);
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename Request::lookup_type>(context);
    }
    if (selection.status == detail::binding_status::found) {
      return runtime_registry_
          .template resolve_binding<typename Request::interface_type, R>(
              selection, context);
    }
    return resolve_missing<
        Request, runtime_auto_constructible_v<typename Request::user_type>,
        decltype(key), R>(context, std::move(key));
  }

  template <typename Request, typename R = typename Request::result_type>
  R construct_runtime_resolved_request(runtime_context &context) {
    try {
      return resolve_request<Request>(context);
    } catch (const type_not_convertible_exception &) {
      using request_value_type = typename Request::value_type;
      auto &&value = resolve_request<request_type<request_value_type>>(context);
      return type_traits<std::decay_t<typename Request::user_type>>::make(
          std::forward<decltype(value)>(value));
    }
  }

  template <typename Request,
            typename Factory = constructor<typename Request::value_type>,
            typename R = typename Request::result_type>
  R construct_runtime_request(runtime_context &context,
                              Factory factory = Factory()) {
    using user_type = typename Request::user_type;
    using request_value_type = typename Request::value_type;

    if constexpr (std::is_same_v<Factory, constructor<request_value_type>>) {
      auto key = detail::no_lookup_key();
      if (runtime_registry_
              .template binding_status<typename Request::lookup_type>(key) !=
          detail::binding_status::not_found) {
        if constexpr (!construct_normalized_request_v<user_type> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          user_type>) {
          return resolve_request<Request>(context);
        } else {
          return construct_runtime_resolved_request<Request, R>(context);
        }
      }
    }

    if constexpr (construct_factory_request_v<user_type>) {
      auto type_guard = context.template track_type<request_value_type>();
      return factory.template construct<R>(context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             user_type>) {
      ::dingo::throw_missing_rvalue_conversion<user_type>(false, context);
    } else {
      return resolve_request<Request>(context);
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

  container_with_static_bindings() : runtime_registry_(this) {}

  explicit container_with_static_bindings(allocator_type alloc)
      : runtime_registry_(this, alloc) {}

  template <typename Parent = ParentContainer,
            std::enable_if_t<!std::is_void_v<Parent>, int> = 0>
  explicit container_with_static_bindings(
      Parent *parent, allocator_type alloc = allocator_type())
      : runtime_registry_(this, alloc), parent_(parent) {}

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
  template <typename Request, typename R, typename LookupKey>
  R resolve_entry(LookupKey key) {
    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context &context) -> R {
          return resolve<Request, R>(context, std::move(key));
        });
  }

public:
  template <typename T, typename IdType = none_t,
            typename R = typename request_type<T, true>::result_type,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename LookupKey,
            typename R = typename request_type<T, true>::result_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  DINGO_ALWAYS_INLINE R resolve(LookupKey key) {
    using request = request_type<T>;
    return resolve_entry<request, R>(std::move(key));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = typename request_type<T, true>::result_type>
  // NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
  DINGO_ALWAYS_INLINE R construct(Factory factory = Factory()) {
    using request = request_type<T>;
    using interface_type = typename request::interface_type;
    using value_type = typename request::value_type;
    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      const bool has_runtime_no_key =
          has_runtime_no_key_binding<interface_type, value_type>();
      if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<T>) {
        using no_key = detail::no_lookup_key_t;
        constexpr bool has_static_normalized_binding =
            is_static_binding_available_v<value_type, no_key>;

        if constexpr (has_static_construct_v<T>) {
          if (!has_runtime_no_key) {
            return execute_transaction(runtime_registry_.runtime(),
                                       [&](runtime_context &context) -> R {
                                         return construct_static<request, R>(
                                             context);
                                       });
          }
        }

        if (has_runtime_no_key) {
          return execute_transaction(
              runtime_registry_.runtime(), [&](runtime_context &context) -> R {
                return construct_runtime_request<request, Factory, R>(
                    context, std::move(factory));
              });
        }

        if constexpr (has_static_normalized_binding) {
          ::dingo::throw_missing_rvalue_conversion<T>(true);
        }

        return execute_transaction(
            runtime_registry_.runtime(), [&](runtime_context &context) -> R {
              return construct_runtime_request<request, Factory, R>(
                  context, std::move(factory));
            });
      } else if constexpr (has_static_construct_v<T>) {
        if (!has_runtime_no_key) {
          return execute_transaction(
              runtime_registry_.runtime(), [&](runtime_context &context) -> R {
                return construct_static<request, R>(context);
              });
        }
      } else {
        return execute_transaction(
            runtime_registry_.runtime(), [&](runtime_context &context) -> R {
              return construct_runtime_request<request, Factory, R>(
                  context, std::move(factory));
            });
      }
    }
    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context &context) -> R {
          if constexpr (std::is_same_v<Factory,
                                       constructor<normalized_type_t<T>>>) {
            auto key = detail::no_lookup_key();
            const auto status = binding_status<T>(key);
            if (status != detail::binding_status::not_found) {
              if constexpr (construct_normalized_request_v<T>) {
                return ::dingo::construct_resolved_request<T>(
                    [&]() {
                      return resolve<request, typename request::lookup_type>(
                          context, key);
                    },
                    [&]() {
                      using normalized_request = request_type<value_type>;
                      return resolve<normalized_request,
                                     typename normalized_request::lookup_type>(
                          context, key);
                    });
              } else {
                return resolve<request, typename request::lookup_type>(context,
                                                                       key);
              }
            } else if (binding_status<value_type>(key) !=
                       detail::binding_status::not_found) {
              if constexpr (construct_normalized_request_v<T>) {
                using normalized_request = request_type<value_type>;
                return type_traits<std::decay_t<T>>::make(
                    resolve<normalized_request,
                            typename normalized_request::lookup_type>(context,
                                                                      key));
              } else {
                return resolve<request, typename request::lookup_type>(context,
                                                                       key);
              }
            }
          }

          if constexpr (construct_factory_request_v<T>) {
            auto type_guard = context.template track_type<value_type>();
            return factory.template construct<R>(context, *this);
          } else {
            return resolve<request, typename request::lookup_type>(
                context, detail::no_lookup_key());
          }
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
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      if (select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            runtime_registry_.runtime(), [&](runtime_context &context) -> T {
              return static_registry_
                  .template construct_collection<T, LookupKey>(*this, context);
            });
      }
    }

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context &context) -> T {
          return construct_collection<T>(
              context, detail::binding_collection_append{}, std::move(key));
        });
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
    if constexpr (detail::is_static_lookup_key_v<LookupKey>) {
      if (select_static_collection_construct<T, LookupKey>()) {
        return execute_transaction(
            runtime_registry_.runtime(), [&](runtime_context &context) -> T {
              return static_registry_
                  .template construct_collection<T, LookupKey>(
                      *this, context, std::forward<Fn>(fn));
            });
      }
    }

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context &context) -> T {
          return construct_collection<T>(context, std::forward<Fn>(fn),
                                         std::move(key));
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

  template <typename Signature = void, typename Callable>
  auto invoke(Callable &&callable) {
    using callable_type = std::remove_cv_t<std::remove_reference_t<Callable>>;
    using dispatch_signature =
        detail::callable_dispatch_signature_t<Signature, callable_type>;

    return execute_transaction(
        runtime_registry_.runtime(), [&](runtime_context &context) {
          auto type_guard = context.template track_type<callable_type>();
          return detail::callable_invoke<dispatch_signature>::construct(
              std::forward<Callable>(callable), context, *this);
        });
  }

  template <typename Request, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  detail::binding_status binding_status(LookupKey key) {
    using request = request_type<Request>;
    using interface_type = typename request::interface_type;
    using static_selection = static_selection_t<interface_type, LookupKey>;
    const auto runtime_status =
        runtime_registry_.template binding_status<interface_type>(key);
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

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn,
                                LookupKey key) {
    return append_collection_impl<T>(results, context, std::move(key),
                                     std::forward<Fn>(fn));
  }

  template <typename T, typename LookupKey, typename Fn,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection_impl(T &results, runtime_context &context,
                                     LookupKey key, Fn &&fn) {
    return detail::append_binding_collection(
        results,
        [&](auto &collection, auto &&append) {
          return runtime_registry_.template append_collection<T>(
              collection, context, std::forward<decltype(append)>(append), key);
        },
        [&](auto &collection, auto &&append) {
          return static_registry_.template append_collection<T, LookupKey>(
              collection, *this, context,
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

private:
  template <typename Request, typename R, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_singular(runtime_context &context, LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    using result_type = R;
    auto runtime_selection =
        runtime_registry_.template select_binding<lookup_type>(key);
    using static_selection =
        typename static_registry_type::template selection<lookup_type,
                                                          LookupKey>;
    if (runtime_selection.status == detail::binding_status::ambiguous ||
        static_selection::status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<lookup_type>(context);
    }
    if constexpr (static_selection::status == detail::binding_status::found) {
      if (runtime_selection.status == detail::binding_status::found) {
        throw detail::make_type_ambiguous_exception<lookup_type>(context);
      }
      return static_registry_
          .template resolve_binding<lookup_type, result_type, static_selection>(
              context, *this);
    } else {
      if (runtime_selection.status == detail::binding_status::found) {
        return runtime_registry_
            .template resolve_binding<result_type, result_type>(
                runtime_selection, context);
      }
      if constexpr (has_parent_v) {
        if (parent_) {
          return resolve_parent<Request>(context, key);
        }
      }
      using user_type = typename Request::user_type;
      return resolve_missing<Request, runtime_auto_constructible_v<user_type>,
                             LookupKey, R>(context, std::move(key));
    }
  }

  template <typename Request, typename R, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(runtime_context &context, LookupKey key) {
    if constexpr (detail::is_static_lookup_key_definition_v<LookupKey>) {
      if constexpr (collection_traits<R>::is_collection) {
        if constexpr (has_parent_v) {
          if (parent_ && count_collection<R>(key) == 0) {
            return resolve_parent<Request>(context, key);
          }
        }
        return construct_collection<R>(
            context, detail::binding_collection_append{}, std::move(key));
      } else {
        return resolve_singular<Request, R>(context, std::move(key));
      }
    } else {
      if constexpr (collection_traits<R>::is_collection) {
        if constexpr (has_parent_v) {
          if (parent_ &&
              runtime_registry_.template count_collection<R>(key) == 0) {
            return resolve_parent<Request>(context, key);
          }
        }
      } else {
        if constexpr (has_parent_v) {
          if (parent_ &&
              runtime_registry_
                      .template binding_status<typename Request::lookup_type>(
                          key) == detail::binding_status::not_found) {
            return resolve_parent<Request>(context, key);
          }
        }
      }
      auto selection =
          runtime_registry_
              .template select_binding<typename Request::lookup_type>(key);
      if (selection.status == detail::binding_status::ambiguous) {
        throw detail::make_type_ambiguous_exception<
            typename Request::lookup_type>(context);
      }
      if (selection.status == detail::binding_status::found) {
        return runtime_registry_
            .template resolve_binding<typename Request::interface_type, R>(
                selection, context);
      }
      return resolve_missing<
          Request, runtime_auto_constructible_v<typename Request::user_type>,
          LookupKey, R>(context, std::move(key));
    }
  }

public:
  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(runtime_context &context, LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    return resolve<request, R>(context, std::move(key));
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
