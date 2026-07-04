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
  template <typename T, typename R, typename IdType>
  R resolve_parent_request(IdType &&id) {
    if constexpr (is_none_v<std::decay_t<IdType>>) {
      return parent_->template resolve<T>();
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      return parent_->template resolve<T>(std::decay_t<IdType>{});
    } else {
      return parent_->template resolve<T>(std::forward<IdType>(id));
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve_parent_request(runtime_context &context) {
    if constexpr (std::is_void_v<Key>) {
      return parent_->template resolve<T, RemoveRvalueReferences>(context);
    } else {
      return parent_->template resolve<T, RemoveRvalueReferences>(
          context, key_type<Key>{});
    }
  }

  template <typename T, bool RemoveRvalueReferences, typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  R resolve_parent_request(runtime_context &context, IdType &&id) {
    return parent_->template resolve<T, RemoveRvalueReferences>(
        context, std::forward<IdType>(id));
  }

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve_missing(runtime_context &context, IdType &&id) {
    return runtime_registry_.template source_missing<
        T, RemoveRvalueReferences, MayAutoConstruct, IdType, R>(
        context, std::forward<IdType>(id));
  }

  template <typename T, typename IdType>
  static constexpr bool can_resolve_missing_from_parent() {
    return has_parent_v &&
           detail::has_lookup<parent_container_type, T, IdType>();
  }

  template <typename R, typename IdType>
  static constexpr bool can_resolve_collection_from_parent() {
    using request_type = typename collection_traits<R>::resolve_type;
    return has_parent_v &&
           detail::has_lookup<parent_container_type, request_type, IdType>();
  }

  template <typename R, typename IdType>
  bool should_resolve_collection_from_parent(IdType &id) {
    return parent_ && runtime_registry_.template count_collection<R>(id) == 0;
  }

  template <typename T, typename IdType>
  bool should_resolve_missing_from_parent(IdType &id,
                                          detail::binding_status status) {
    return parent_ && status == detail::binding_status::not_found &&
           detail::should_resolve_missing_from_parent<T>(*parent_, id);
  }

  template <typename T, bool RemoveRvalueReferences, typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve_context_parent_request(runtime_context &context, IdType &&id) {
    if constexpr (is_none_v<std::decay_t<IdType>>) {
      return resolve_parent_request<T, RemoveRvalueReferences>(context);
    } else if constexpr (detail::is_typed_key_v<IdType>) {
      return resolve_parent_request<T, RemoveRvalueReferences,
                                    typename std::decay_t<IdType>::type>(
          context);
    } else {
      return resolve_parent_request<T, RemoveRvalueReferences>(
          context, std::forward<IdType>(id));
    }
  }

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve_binding(typename registry_type::runtime_selection selection,
                    runtime_context &context, IdType &&id) {
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<T>(context);
    }
    if (selection.status == detail::binding_status::found) {
      using request_type = dependency_interface_t<T>;
      return runtime_registry_.template resolve_binding<request_type, R>(
          selection, context);
    }
    return resolve_missing<T, RemoveRvalueReferences, MayAutoConstruct, IdType,
                           R>(context, std::forward<IdType>(id));
  }

  template <typename T, typename IdType, typename R = dependency_result_t<T>>
  R resolve_binding(typename registry_type::runtime_selection selection,
                    runtime_context &context, IdType &&id) {
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<T>(context);
    }
    if (selection.status == detail::binding_status::found) {
      using request_type = dependency_interface_t<T>;
      return runtime_registry_.template resolve_binding<request_type, R>(
          selection, context);
    }
    return resolve_missing<T, true, false, IdType, R>(context,
                                                      std::forward<IdType>(id));
  }

  template <typename T, bool RemoveRvalueReferences, bool MayAutoConstruct,
            typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve_request(runtime_context &context, IdType &&id) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (is_none_v<std::decay_t<IdType>>) {
        auto selection = runtime_registry_.template select_binding<T>();
        if (selection.status == detail::binding_status::ambiguous) {
          throw detail::make_type_ambiguous_exception<T>(context);
        }
        if (selection.status == detail::binding_status::found) {
          using request_type = dependency_interface_t<T>;
          return runtime_registry_.template resolve_binding<request_type, R>(
              selection, context);
        }
      }
      if constexpr (can_resolve_collection_from_parent<R, IdType>()) {
        if (should_resolve_collection_from_parent<R>(id)) {
          return resolve_context_parent_request<T, RemoveRvalueReferences>(
              context, std::forward<IdType>(id));
        }
      }
      return runtime_registry_.template resolve<T, RemoveRvalueReferences>(
          context, std::forward<IdType>(id));
    } else {
      auto selection = runtime_registry_.template select_binding<T>(id);
      if constexpr (can_resolve_missing_from_parent<T, IdType>()) {
        if (should_resolve_missing_from_parent<T>(id, selection.status)) {
          return resolve_context_parent_request<T, RemoveRvalueReferences>(
              context, std::forward<IdType>(id));
        }
      }
      return resolve_binding<T, RemoveRvalueReferences, MayAutoConstruct,
                             IdType, R>(selection, context,
                                        std::forward<IdType>(id));
    }
  }

  template <typename T, typename IdType, typename R = dependency_result_t<T>>
  R resolve_request(IdType &&id) {
    if constexpr (collection_traits<R>::is_collection) {
      if constexpr (is_none_v<std::decay_t<IdType>>) {
        runtime_context context;
        auto selection = runtime_registry_.template select_binding<T>();
        if (selection.status == detail::binding_status::ambiguous) {
          throw detail::make_type_ambiguous_exception<T>(context);
        }
        if (selection.status == detail::binding_status::found) {
          using request_type = dependency_interface_t<T>;
          return runtime_registry_.template resolve_binding<request_type, R>(
              selection, context);
        }
      }
      if constexpr (can_resolve_collection_from_parent<R, IdType>()) {
        if (should_resolve_collection_from_parent<R>(id)) {
          return resolve_parent_request<T, R>(std::forward<IdType>(id));
        }
      }
      return runtime_registry_.template resolve<T>(std::forward<IdType>(id));
    } else {
      runtime_context context;
      auto selection = runtime_registry_.template select_binding<T>(id);
      if constexpr (can_resolve_missing_from_parent<T, IdType>()) {
        if (should_resolve_missing_from_parent<T>(id, selection.status)) {
          return resolve_parent_request<T, R>(std::forward<IdType>(id));
        }
      }
      return resolve_binding<T, IdType, R>(selection, context,
                                           std::forward<IdType>(id));
    }
  }

  template <typename T, typename R = resolve_dependency_t<T, false>>
  R resolve_request(runtime_context &context) {
    auto selection = runtime_registry_.template select_binding<T>();
    if (selection.status == detail::binding_status::ambiguous) {
      throw detail::make_type_ambiguous_exception<T>(context);
    }
    if (selection.status == detail::binding_status::found) {
      using request_type = dependency_interface_t<T>;
      return runtime_registry_.template resolve_binding<request_type, R>(
          selection, context);
    }
    if constexpr (can_resolve_missing_from_parent<T, none_t>()) {
      none_t id;
      if (should_resolve_missing_from_parent<T>(id, selection.status)) {
        return resolve_parent_request<T, false>(context);
      }
    }
    return resolve_missing<
        T, false, detail::is_runtime_auto_constructible_dependency_v<T>, none_t,
        R>(context, none_t{});
  }

  template <typename T, typename R = dependency_result_t<T>>
  R construct_resolved_request(runtime_context &context) {
    try {
      return resolve_request<T>(context);
    } catch (const type_not_convertible_exception &) {
      auto &&value = resolve_request<normalized_type_t<T>>(context);
      return type_traits<std::decay_t<T>>::make(
          std::forward<decltype(value)>(value));
    }
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = dependency_result_t<T>>
  R construct_request(Factory factory = Factory()) {
    runtime_context context;

    if constexpr (std::is_same_v<Factory, constructor<normalized_type_t<T>>>) {
      if (binding_status<T>() != detail::binding_status::not_found) {
        if constexpr (!construct_normalized_request_v<T> ||
                      ::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          return resolve_request<T>(context);
        } else {
          return construct_resolved_request<T, R>(context);
        }
      } else if (binding_status<normalized_type_t<T>>() !=
                 detail::binding_status::not_found) {
        if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                          T>) {
          ::dingo::throw_missing_rvalue_conversion<T>(true, context);
        } else if constexpr (construct_normalized_request_v<T>) {
          return type_traits<std::decay_t<T>>::make(
              resolve_request<normalized_type_t<T>>(context));
        } else {
          return resolve_request<T>(context);
        }
      }
    }

    if constexpr (construct_factory_request_v<T>) {
      return factory.template construct<R>(context, *this);
    } else if constexpr (::dingo::rvalue_request_requires_explicit_conversion_v<
                             T>) {
      ::dingo::throw_missing_rvalue_conversion<T>(false, context);
    } else {
      return resolve_request<T>(context);
    }
  }

public:
  template <typename T, typename IdType>
  detail::binding_status binding_status(IdType &&id) {
    auto status = runtime_registry_.template binding_status<T>(id);
    if constexpr (can_resolve_missing_from_parent<T, IdType>()) {
      if (parent_ && status == detail::binding_status::not_found) {
        return parent_->template binding_status<T>(id);
      }
    }
    return status;
  }

  template <typename T, typename IdType = none_t,
            typename R = dependency_result_t<T>>
  R resolve(IdType &&id = IdType()) {
    return resolve_request<T>(std::forward<IdType>(id));
  }

  template <typename T, bool RemoveRvalueReferences,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context) {
    return resolve_request<
        T, RemoveRvalueReferences,
        detail::is_runtime_auto_constructible_dependency_v<T>>(context,
                                                               none_t{});
  }

  template <typename T, bool RemoveRvalueReferences,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, none_t) {
    return resolve_request<
        T, RemoveRvalueReferences,
        detail::is_runtime_auto_constructible_dependency_v<T>>(context,
                                                               none_t{});
  }

  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, key_type<Key>) {
    return resolve_request<
        T, RemoveRvalueReferences,
        detail::is_runtime_auto_constructible_dependency_v<T>>(context,
                                                               key_type<Key>{});
  }

  template <typename T, bool RemoveRvalueReferences, typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  R resolve(runtime_context &context, IdType &&id) {
    return resolve_request<
        T, RemoveRvalueReferences,
        detail::is_runtime_auto_constructible_dependency_v<T>>(
        context, std::forward<IdType>(id));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = dependency_result_t<T>>
  R construct(Factory factory = Factory()) {
    return construct_request<T>(std::move(factory));
  }

  template <typename T> T construct_collection() {
    return runtime_registry_.template construct_collection<T>();
  }

  template <typename T, typename Fn> T construct_collection(Fn &&fn) {
    return runtime_registry_.template construct_collection<T>(
        std::forward<Fn>(fn));
  }

  template <typename T, typename Key> T construct_collection(key_type<Key>) {
    return runtime_registry_.template construct_collection<T>(key_type<Key>{});
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key_type<Key>) {
    return runtime_registry_.template construct_collection<T>(
        std::forward<Fn>(fn), key_type<Key>{});
  }

  template <typename T, typename Fn> T construct_collection(Fn &&fn, none_t) {
    return runtime_registry_.template construct_collection<T>(
        std::forward<Fn>(fn));
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

  template <typename Request, typename Key = void>
  detail::binding_status binding_status() {
    return runtime_registry_.template binding_status<Request, Key>();
  }

  template <typename T, typename Key = void, typename Fn>
  std::size_t append_collection(T &results, runtime_context &context, Fn &&fn) {
    return runtime_registry_.template append_collection<T, Key>(
        results, context, std::forward<Fn>(fn));
  }

  template <typename T, typename Key = void> std::size_t count_collection() {
    return runtime_registry_.template count_collection<T, Key>();
  }

  registry_type runtime_registry_;
  parent_container_type *parent_ = nullptr;
};

} // namespace dingo

#ifdef _MSC_VER
#pragma warning(pop)
#endif
