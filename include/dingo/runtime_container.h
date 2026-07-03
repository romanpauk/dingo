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

namespace dingo {

namespace detail {

template <typename ParentContainer, typename Request, typename IdType,
          typename = void>
struct has_parent_key_value_lookup_definition : std::false_type {};

template <typename ParentContainer, typename Request, typename IdType>
struct has_parent_key_value_lookup_definition<
    ParentContainer, Request, IdType,
    std::void_t<typename ParentContainer::lookup_definition_type>>
    : std::bool_constant<!std::is_void_v<selected_lookup_entry_t<
          normalized_type_t<Request>, std::decay_t<IdType>,
          normalize_lookup_definitions_t<
              typename ParentContainer::lookup_definition_type>>>> {};

template <typename ParentContainer, typename Request, typename IdType>
inline constexpr bool has_parent_key_value_lookup_definition_v =
    has_parent_key_value_lookup_definition<ParentContainer, Request,
                                           IdType>::value;

template <typename T>
inline constexpr bool is_runtime_auto_constructible_dependency_v =
    std::is_same_v<dependency_value_t<T>, std::decay_t<T>> &&
    (!std::is_reference_v<T> ||
     (std::is_lvalue_reference_v<T> &&
      std::is_const_v<std::remove_reference_t<T>> &&
      is_auto_constructible<std::decay_t<T>>::value));

template <typename T, typename = void>
struct has_static_registry_type : std::false_type {};

template <typename T>
struct has_static_registry_type<T,
                                std::void_t<typename T::static_registry_type>>
    : std::true_type {};

template <typename T>
inline constexpr bool has_static_registry_type_v =
    has_static_registry_type<T>::value;

template <typename ParentContainer, typename Request, typename IdType,
          typename = void>
struct has_parent_runtime_binding_status : std::false_type {};

template <typename ParentContainer, typename Request, typename IdType>
struct has_parent_runtime_binding_status<
    ParentContainer, Request, IdType,
    std::void_t<decltype(std::declval<ParentContainer &>()
                             .template runtime_binding_status_for_id<Request>(
                                 std::declval<IdType &>()))>> : std::true_type {
};

template <typename ParentContainer, typename Request, typename IdType>
inline constexpr bool has_parent_runtime_binding_status_v =
    has_parent_runtime_binding_status<ParentContainer, Request, IdType>::value;

template <typename ParentContainer, typename Request, typename IdType>
constexpr bool has_parent_lookup_for_id() {
  if constexpr (is_none_v<std::decay_t<IdType>> || is_typed_key_v<IdType>) {
    return true;
  } else {
    return has_parent_key_value_lookup_definition_v<ParentContainer, Request,
                                                    IdType>;
  }
}

template <typename Request, typename ParentContainer, typename IdType>
bool should_resolve_missing_from_parent(ParentContainer &parent, IdType &id) {
  if constexpr (is_runtime_auto_constructible_dependency_v<Request> &&
                !has_static_registry_type_v<ParentContainer> &&
                has_parent_runtime_binding_status_v<ParentContainer, Request,
                                                    IdType>) {
    return parent.template runtime_binding_status_for_id<Request>(id) !=
           binding_selection_status::not_found;
  } else {
    return true;
  }
}

} // namespace detail

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

  registry_type &registry() { return runtime_registry_; }

  const registry_type &registry() const { return runtime_registry_; }

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

  template <typename T, typename IdType>
  detail::binding_selection_status runtime_binding_status_for_id(IdType &&id) {
    return runtime_registry_.template binding_status_for_id<T>(
        std::forward<IdType>(id));
  }

  template <typename T, typename IdType = none_t,
            typename R = dependency_result_t<T>>
  R resolve(IdType &&id = IdType()) {
    if constexpr (collection_traits<R>::is_collection) {
      if (parent_ &&
          runtime_registry_.template count_runtime_collection<R>(id) == 0) {
        return resolve_parent_request<T, R>(std::forward<IdType>(id));
      }
    } else {
      if (parent_ && runtime_registry_.template binding_status_for_id<T>(id) ==
                         detail::binding_selection_status::not_found) {
        if constexpr (detail::has_parent_lookup_for_id<parent_container_type, T,
                                                       IdType>()) {
          if (detail::should_resolve_missing_from_parent<T>(*parent_, id)) {
            return resolve_parent_request<T, R>(std::forward<IdType>(id));
          }
        }
      }
    }
    return runtime_registry_.template resolve<T>(std::forward<IdType>(id));
  }

  template <typename T, bool RemoveRvalueReferences,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context) {
    if (parent_ &&
        runtime_registry_.template binding_status_for_id<T>(none_t{}) ==
            detail::binding_selection_status::not_found) {
      none_t id;
      if (!detail::should_resolve_missing_from_parent<T>(*parent_, id)) {
        return runtime_registry_
            .template resolve_request<T, RemoveRvalueReferences>(context);
      }
      return parent_->template resolve<T, RemoveRvalueReferences>(context);
    }
    return runtime_registry_
        .template resolve_request<T, RemoveRvalueReferences>(context);
  }

  template <typename T, bool RemoveRvalueReferences,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, none_t) {
    return resolve<T, RemoveRvalueReferences>(context);
  }

  template <typename T, bool RemoveRvalueReferences, typename Key,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve(runtime_context &context, key_type<Key>) {
    if (parent_ &&
        runtime_registry_.template binding_status_for_id<T>(key_type<Key>{}) ==
            detail::binding_selection_status::not_found) {
      key_type<Key> id;
      if (!detail::should_resolve_missing_from_parent<T>(*parent_, id)) {
        return runtime_registry_
            .template resolve_request<T, RemoveRvalueReferences, Key>(context);
      }
      return parent_->template resolve<T, RemoveRvalueReferences>(
          context, key_type<Key>{});
    }
    return runtime_registry_
        .template resolve_request<T, RemoveRvalueReferences, Key>(context);
  }

  template <typename T, bool RemoveRvalueReferences, typename IdType,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>,
            std::enable_if_t<!is_none_v<std::decay_t<IdType>> &&
                                 !detail::is_typed_key_v<IdType>,
                             int> = 0>
  R resolve(runtime_context &context, IdType &&id) {
    if (parent_ && runtime_registry_.template binding_status_for_id<T>(id) ==
                       detail::binding_selection_status::not_found) {
      if constexpr (detail::has_parent_lookup_for_id<parent_container_type, T,
                                                     IdType>()) {
        if (detail::should_resolve_missing_from_parent<T>(*parent_, id)) {
          return parent_->template resolve<T, RemoveRvalueReferences>(
              context, std::forward<IdType>(id));
        }
      }
    }
    return runtime_registry_.template resolve<T, RemoveRvalueReferences>(
        context, std::forward<IdType>(id));
  }

  template <typename T, typename Factory = constructor<normalized_type_t<T>>,
            typename R = dependency_result_t<T>>
  R construct(Factory factory = Factory()) {
    return runtime_registry_.template construct<T>(std::move(factory));
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
  detail::binding_selection_status binding_status() {
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

  template <typename T, bool RemoveRvalueReferences, typename Key = void,
            typename R = resolve_dependency_t<T, RemoveRvalueReferences>>
  R resolve_request(runtime_context &context) {
    return runtime_registry_
        .template resolve_request<T, RemoveRvalueReferences, Key>(context);
  }

  registry_type runtime_registry_;
  parent_container_type *parent_ = nullptr;
};

} // namespace dingo
