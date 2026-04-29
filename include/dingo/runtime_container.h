//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/runtime/registry.h>
#include <dingo/type/request_traits.h>

namespace dingo {

template <typename ContainerTraits, typename Allocator, typename ParentContainer>
class runtime_container
    : public runtime_registry<
          ContainerTraits, Allocator,
          std::conditional_t<std::is_same_v<void, ParentContainer>, void,
                             detail::runtime_parent_registry_t<ParentContainer>>,
          void> {
    using registry_base = runtime_registry<
        ContainerTraits, Allocator,
        std::conditional_t<std::is_same_v<void, ParentContainer>, void,
                           detail::runtime_parent_registry_t<ParentContainer>>,
        void>;

  public:
    using container_traits_type = ContainerTraits;
    using allocator_type = Allocator;
    using container_type =
        runtime_container<ContainerTraits, Allocator, ParentContainer>;
    using registry_type = registry_base;
    using parent_container_type =
        std::conditional_t<std::is_same_v<void, ParentContainer>, container_type,
                           ParentContainer>;
    using runtime_injector_type = typename registry_type::runtime_injector_type;

    using registry_type::get_allocator;
    using registry_type::register_indexed_type;
    using registry_type::register_type;
    using registry_type::register_type_collection;

    template <typename ContainerTraitsT, typename AllocatorT,
              typename ParentContainerT>
    using rebind_t =
        runtime_container<ContainerTraitsT, AllocatorT, ParentContainerT>;

    template <typename Tag>
    using child_container_type =
        runtime_container<typename container_traits_type::template rebind_t<
                              type_list<typename container_traits_type::tag_type,
                                        Tag>>,
                          Allocator, container_type>;

    runtime_container() = default;

    explicit runtime_container(allocator_type alloc) : registry_type(alloc) {}

    runtime_container(parent_container_type* parent,
                      allocator_type alloc = allocator_type())
        : registry_type(detail::runtime_parent_registry_ptr(parent), alloc) {}

    registry_type& registry() { return *this; }

    const registry_type& registry() const { return *this; }

    runtime_injector_type injector() { return registry().injector(); }

    template <typename T, typename IdType = none_t,
              typename R = request_result_t<T>>
    R resolve(IdType&& id = IdType()) {
        return injector().template resolve<T>(std::forward<IdType>(id));
    }

    template <typename T,
              typename Factory = constructor<normalized_type_t<T>>,
              typename R = request_result_t<T>>
    R construct(Factory factory = Factory()) {
        return injector().template construct<T>(std::move(factory));
    }

    template <typename T> T construct_collection() {
        return injector().template construct_collection<T>();
    }

    template <typename T, typename Fn> T construct_collection(Fn&& fn) {
        return injector().template construct_collection<T>(
            std::forward<Fn>(fn));
    }

    template <typename T, typename Key> T construct_collection(key<Key>) {
        return injector().template construct_collection<T>(key<Key>{});
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection(Fn&& fn, key<Key>) {
        return injector().template construct_collection<T>(
            std::forward<Fn>(fn), key<Key>{});
    }

    template <typename T, typename Fn>
    T construct_collection(Fn&& fn, none_t) {
        return injector().template construct_collection<T>(
            std::forward<Fn>(fn));
    }

    template <typename Signature = void, typename Callable>
    auto invoke(Callable&& callable) {
        return injector().template invoke<Signature>(
            std::forward<Callable>(callable));
    }
};

} // namespace dingo
