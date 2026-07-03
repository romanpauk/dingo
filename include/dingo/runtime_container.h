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
        return resolve_parent_request<T, R>(std::forward<IdType>(id));
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
  R resolve(runtime_context &context, key<Key>) {
    if (parent_ &&
        runtime_registry_.template binding_status_for_id<T>(key<Key>{}) ==
            detail::binding_selection_status::not_found) {
      return parent_->template resolve<T, RemoveRvalueReferences>(context,
                                                                  key<Key>{});
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
      return parent_->template resolve<T, RemoveRvalueReferences>(
          context, std::forward<IdType>(id));
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

  template <typename T, typename Key> T construct_collection(key<Key>) {
    return runtime_registry_.template construct_collection<T>(key<Key>{});
  }

  template <typename T, typename Fn, typename Key>
  T construct_collection(Fn &&fn, key<Key>) {
    return runtime_registry_.template construct_collection<T>(
        std::forward<Fn>(fn), key<Key>{});
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
