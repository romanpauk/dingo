//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/registration_api.h>
#include <dingo/runtime/registry.h>
#include <dingo/type/dependency_traits.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)
#endif

namespace dingo {

template <typename ContainerTraits = dynamic_container_traits,
          typename Allocator = typename ContainerTraits::allocator_type,
          typename ParentContainer = void>
class runtime_container
    : public detail::runtime_registration_api<
          runtime_container<ContainerTraits, Allocator, ParentContainer>> {
  using self_type =
      runtime_container<ContainerTraits, Allocator, ParentContainer>;
  using registry_base =
      runtime_registry<ContainerTraits, Allocator, void, self_type>;

  friend class runtime_context;
  template <typename, typename> friend class detail::binding_resolution;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using container_type = self_type;
  using registry_type = registry_base;
  using parent_container_type =
      std::conditional_t<std::is_same_v<void, ParentContainer>, container_type,
                         ParentContainer>;
  using rtti_type = typename registry_type::rtti_type;
  using lookup_definition_type = typename registry_type::lookup_definition_type;
  static constexpr bool has_parent_v =
      !std::is_same_v<void, parent_container_type>;

  template <typename ContainerTraitsT, typename AllocatorT,
            typename ParentContainerT>
  using rebind_t =
      runtime_container<ContainerTraitsT, AllocatorT, ParentContainerT>;

  template <typename Tag>
  using child_container_type = runtime_container<
      typename container_traits_type::template rebind_t<
          type_list<typename container_traits_type::tag_type, Tag>>,
      Allocator, container_type>;

  runtime_container() : runtime_registry_(this) {}

  explicit runtime_container(const allocator_type &alloc)
      : runtime_registry_(this, alloc) {}

  runtime_container(parent_container_type *parent,
                    const allocator_type &alloc = allocator_type())
      : runtime_registry_(this, alloc), parent_(parent) {}

private:
  template <typename Request, typename R, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(LookupKey key) {
    return parent_->template resolve<typename Request::user_type>(
        std::move(key));
  }

  template <typename Request, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_parent(runtime_context &context, LookupKey key) {
    return parent_->template resolve<typename Request::user_type,
                                     Request::removes_rvalue_references>(
        context, std::move(key));
  }

  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_missing(runtime_context &context, const LookupKey &key) {
    return runtime_registry_
        .template source_missing<Request, MayAutoConstruct, LookupKey, R>(
            context, key);
  }

  template <typename Request, typename LookupKey>
  static constexpr bool can_resolve_missing_from_parent() {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return has_parent_v &&
           detail::has_lookup<parent_container_type,
                              typename Request::lookup_type, LookupKey>();
  }

  template <typename R, typename LookupKey>
  static constexpr bool can_resolve_collection_from_parent() {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    using collection_request = typename collection_traits<R>::resolve_type;
    return has_parent_v && detail::has_lookup<parent_container_type,
                                              collection_request, LookupKey>();
  }

  template <typename R, typename LookupKey>
  bool should_resolve_collection_from_parent(LookupKey &key) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return parent_ && runtime_registry_.template count_collection<R>(key) == 0;
  }

  template <typename Request, typename LookupKey>
  bool should_resolve_missing_from_parent(LookupKey &key,
                                          detail::binding_status status) {
    static_assert(detail::is_lookup_key_v<LookupKey>);
    return parent_ && status == detail::binding_status::not_found &&
           detail::should_resolve_missing_from_parent<
               typename Request::lookup_type>(*parent_, key);
  }

  template <typename Request, bool MayAutoConstruct, typename LookupKey,
            typename R = typename Request::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_binding(typename registry_type::runtime_selection selection,
                    runtime_context &context, LookupKey key) {
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename Request::lookup_type>(context);
    }
    if (selection.status == detail::binding_status::found) {
      return runtime_registry_
          .template resolve_binding<typename Request::interface_type, R>(
              selection, context);
    }
    return resolve_missing<Request, MayAutoConstruct, LookupKey, R>(
        context, std::move(key));
  }

  template <typename Request, typename LookupKey,
            typename R = typename Request::result_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_binding(typename registry_type::runtime_selection selection,
                    runtime_context &context, const LookupKey &key) {
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<
          typename Request::lookup_type>(context);
    }
    if (selection.status == detail::binding_status::found) {
      return runtime_registry_
          .template resolve_binding<typename Request::interface_type, R>(
              selection, context);
    }
    return resolve_missing<Request, false, LookupKey, R>(context,
                                                         std::move(key));
  }

  template <typename T, typename LookupKey>
  typename registry_type::runtime_selection
  select_collection_binding(runtime_context &context, LookupKey &key) {
    if constexpr (detail::is_no_lookup_key_v<LookupKey>) {
      auto selection = runtime_registry_.template select_binding<T>(key);
      if (selection.status == detail::binding_status::ambiguous) {
        throw detail::make_type_ambiguous_exception<T>(context);
      }
      return selection;
    } else {
      (void)context;
      (void)key;
      return {};
    }
  }

  template <typename Request, bool MayAutoConstruct, typename R,
            typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_request(runtime_context &context, LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    if constexpr (collection_traits<R>::is_collection) {
      auto selection = select_collection_binding<lookup_type>(context, key);
      if (selection.status == detail::binding_status::found) {
        return runtime_registry_
            .template resolve_binding<typename Request::interface_type, R>(
                selection, context);
      }
      if constexpr (can_resolve_collection_from_parent<R, LookupKey>()) {
        if (should_resolve_collection_from_parent<R>(key)) {
          return resolve_parent<Request>(context, key);
        }
      }
      return runtime_registry_.template resolve<
          typename Request::user_type, Request::removes_rvalue_references>(
          context, std::move(key));
    } else {
      auto selection =
          runtime_registry_.template select_binding<lookup_type>(key);
      if constexpr (can_resolve_missing_from_parent<Request, LookupKey>()) {
        if (should_resolve_missing_from_parent<Request>(key,
                                                        selection.status)) {
          return resolve_parent<Request>(context, key);
        }
      }
      return resolve_binding<Request, MayAutoConstruct, LookupKey, R>(
          selection, context, std::move(key));
    }
  }

  template <typename Request, typename R, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve_request(LookupKey key) {
    using lookup_type = typename Request::lookup_type;
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (detail::is_no_lookup_key_v<LookupKey>) {
        runtime_context context;
        auto selection = select_collection_binding<lookup_type>(context, key);
        if (selection.status == detail::binding_status::found) {
          return runtime_registry_
              .template resolve_binding<typename Request::interface_type, R>(
                  selection, context);
        }
      }
      if constexpr (can_resolve_collection_from_parent<R, LookupKey>()) {
        if (should_resolve_collection_from_parent<R>(key)) {
          return resolve_parent<Request, R>(key);
        }
      }
      return runtime_registry_.template resolve<typename Request::user_type>(
          std::move(key));
    } else {
      runtime_context context;
      auto selection =
          runtime_registry_.template select_binding<lookup_type>(key);
      if constexpr (can_resolve_missing_from_parent<Request, LookupKey>()) {
        if (should_resolve_missing_from_parent<Request>(key,
                                                        selection.status)) {
          return resolve_parent<Request, R>(key);
        }
      }
      return resolve_binding<Request, LookupKey, R>(selection, context,
                                                    std::move(key));
    }
  }

  template <typename Request, typename R = typename Request::result_type>
  R construct_resolved_request(runtime_context &context) {
    try {
      return resolve_request<Request,
                             detail::is_runtime_auto_constructible_dependency_v<
                                 typename Request::user_type>,
                             R>(context, detail::no_lookup_key());
    } catch (const type_not_convertible_exception &) {
      using value_type = typename Request::value_type;
      auto &&value =
          resolve_request<request_type<value_type>, true,
                          typename request_type<value_type>::lookup_type>(
              context, detail::no_lookup_key());
      return type_traits<std::decay_t<typename Request::user_type>>::make(
          std::forward<decltype(value)>(value));
    }
  }

  template <typename Request,
            typename Factory = constructor<typename Request::value_type>,
            typename R = typename Request::result_type>
  R construct_request(Factory factory = Factory()) {
    using user_type = typename Request::user_type;
    using value_type = typename Request::value_type;
    runtime_context context;

    if constexpr (std::is_same_v<Factory, constructor<value_type>>) {
      auto key = detail::no_lookup_key();
      if (binding_status<typename Request::lookup_type>(key) !=
          detail::binding_status::not_found) {
        if constexpr (!construct_normalized_request_v<user_type> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          user_type>) {
          return resolve_request<
              Request,
              detail::is_runtime_auto_constructible_dependency_v<user_type>, R>(
              context, key);
        } else {
          return construct_resolved_request<Request, R>(context);
        }
      } else if (binding_status<value_type>(key) !=
                 detail::binding_status::not_found) {
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          user_type>) {
          ::dingo::throw_missing_rvalue_conversion<user_type>(true, context);
        } else if constexpr (construct_normalized_request_v<user_type>) {
          return type_traits<std::decay_t<user_type>>::make(
              resolve_request<request_type<value_type>, true,
                              typename request_type<value_type>::lookup_type>(
                  context, key));
        } else {
          return resolve_request<
              Request,
              detail::is_runtime_auto_constructible_dependency_v<user_type>, R>(
              context, key);
        }
      }
    }

    if constexpr (construct_factory_request_v<user_type>) {
      return factory.template construct<R>(context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             user_type>) {
      ::dingo::throw_missing_rvalue_conversion<user_type>(false, context);
    } else {
      auto key = detail::no_lookup_key();
      return resolve_request<
          Request,
          detail::is_runtime_auto_constructible_dependency_v<user_type>, R>(
          context, key);
    }
  }

public:
  template <typename T, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    auto status = runtime_registry_.template binding_status<T>(key);
    if constexpr (can_resolve_missing_from_parent<request, decltype(key)>()) {
      if (parent_ && status == detail::binding_status::not_found) {
        return parent_->template binding_status<T>(key);
      }
    }
    return status;
  }

  template <typename T, typename IdType = none_t,
            typename R = typename request_type<T, true>::result_type,
            std::enable_if_t<!detail::is_lookup_key_v<IdType>, int> = 0>
  R resolve(IdType &&id = IdType()) {
    using request = request_type<T>;
    auto key = detail::make_lookup_key(std::forward<IdType>(id));
    return resolve_request<request, R>(std::move(key));
  }

  template <typename T, typename LookupKey,
            typename R = typename request_type<T, true>::result_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(LookupKey key) {
    using request = request_type<T>;
    return resolve_request<request, R>(std::move(key));
  }

  template <typename T, bool RemoveRvalueReferences, typename LookupKey,
            typename R =
                typename request_type<T, RemoveRvalueReferences>::lookup_type,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  R resolve(runtime_context &context, LookupKey key) {
    using request = request_type<T, RemoveRvalueReferences>;
    return resolve_request<
        request, detail::is_runtime_auto_constructible_dependency_v<T>, R>(
        context, key);
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = typename request_type<T, true>::result_type>
  R construct(Factory factory = Factory()) {
    return construct_request<request_type<T>, Factory, R>(std::move(factory));
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
    return runtime_registry_.template construct_collection<T>(
        detail::binding_collection_append{}, std::move(key));
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  T construct_collection(Fn &&fn, LookupKey key) {
    return runtime_registry_.template construct_collection<T>(
        std::forward<Fn>(fn), std::move(key));
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
    return runtime_registry_.template invoke<Signature>(
        std::forward<Callable>(callable));
  }

private:
  friend class detail::runtime_registration_api<self_type>;

  registry_type &binding_store() { return runtime_registry_; }

  self_type &runtime_registration_parent() { return *this; }

  template <typename Request, typename Key>
  detail::binding_status binding_status() {
    return runtime_registry_.template binding_status<Request>(Key{});
  }

  template <typename T, typename Key, typename Fn>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn) {
    return runtime_registry_.template append_collection<T>(
        results, context, std::forward<Fn>(fn), Key{});
  }

  template <typename T, typename Fn, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn,
                                LookupKey key) {
    return runtime_registry_.template append_collection<T>(
        results, context, std::forward<Fn>(fn), std::move(key));
  }

  template <typename T, typename Key> std::size_t count_collection() {
    return runtime_registry_.template count_collection<T>(Key{});
  }

  template <typename T, typename LookupKey,
            std::enable_if_t<detail::is_lookup_key_v<LookupKey>, int> = 0>
  std::size_t count_collection(LookupKey key) {
    return runtime_registry_.template count_collection<T>(std::move(key));
  }

  registry_type runtime_registry_;
  parent_container_type *parent_ = nullptr;
};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
