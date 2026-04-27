//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/container_common.h>
#include <dingo/core/keyed.h>
#include <dingo/factory/constructor.h>
#include <dingo/registration/annotated.h>
#include <dingo/type/normalized_type.h>

#include <type_traits>
#include <utility>

namespace dingo {

template <typename Registry>
class runtime_injector {
  public:
    using registry_type = Registry;

    explicit runtime_injector(registry_type& registry)
        : registry_(&registry) {}

    template <typename T, typename IdType = none_t,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R resolve(IdType&& id = IdType()) {
        return registry_->template resolve_runtime_request<T>(
            std::forward<IdType>(id));
    }

    template <typename T,
              typename Factory = constructor<normalized_type_t<T>>,
              typename R = typename annotated_traits<std::conditional_t<
                  std::is_rvalue_reference_v<T>, std::remove_reference_t<T>,
                  T>>::type>
    R construct(Factory factory = Factory()) {
        return registry_->template construct_runtime_request<T>(
            std::move(factory));
    }

    template <typename T>
    T construct_collection() {
        return registry_->template construct_collection_runtime_request<T>();
    }

    template <typename T, typename Fn>
    T construct_collection(Fn&& fn) {
        return registry_->template construct_collection_runtime_request<T>(
            std::forward<Fn>(fn));
    }

    template <typename T, typename Key>
    T construct_collection(key<Key>) {
        return registry_->template construct_collection_runtime_request<T>(
            key<Key>{});
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection(Fn&& fn, key<Key>) {
        return registry_->template construct_collection_runtime_request<T>(
            std::forward<Fn>(fn), key<Key>{});
    }

    template <typename Signature = void, typename Callable>
    auto invoke(Callable&& callable) {
        return registry_->template invoke_runtime_request<Signature>(
            std::forward<Callable>(callable));
    }

    registry_type& registry() { return *registry_; }

    registry_type& container() { return registry(); }

  private:
    registry_type* registry_;
};

} // namespace dingo
