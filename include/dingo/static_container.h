//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>
#include <dingo/static/injector.h>

namespace dingo {

template <typename StaticSource>
class static_container {
  public:
    using source_type = StaticSource;
    using injector_type = static_injector<StaticSource>;
    using static_source_type = typename injector_type::static_source_type;

    injector_type& injector() { return injector_; }

    const injector_type& injector() const { return injector_; }

    template <typename T,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R resolve() {
        return injector_.template resolve<T>();
    }

    template <typename T, typename Key,
              typename R = typename annotated_traits<
                  std::conditional_t<std::is_rvalue_reference_v<T>,
                                     std::remove_reference_t<T>, T>>::type>
    R resolve(key<Key>) {
        return injector_.template resolve<T>(key<Key>{});
    }

    template <typename T,
              typename Factory = constructor<normalized_type_t<T>>,
              typename R = typename annotated_traits<std::conditional_t<
                  std::is_rvalue_reference_v<T>, std::remove_reference_t<T>,
                  T>>::type>
    R construct(Factory factory = Factory()) {
        return injector_.template construct<T>(std::move(factory));
    }

    template <typename T> T construct_collection() {
        return injector_.template construct_collection<T>();
    }

    template <typename T, typename Fn> T construct_collection(Fn&& fn) {
        return injector_.template construct_collection<T>(
            std::forward<Fn>(fn));
    }

    template <typename T, typename Key> T construct_collection(key<Key>) {
        return injector_.template construct_collection<T>(key<Key>{});
    }

    template <typename T, typename Fn, typename Key>
    T construct_collection(Fn&& fn, key<Key>) {
        return injector_.template construct_collection<T>(
            std::forward<Fn>(fn), key<Key>{});
    }

    template <typename Signature = void, typename Callable>
    auto invoke(Callable&& callable) {
        return injector_.template invoke<Signature>(
            std::forward<Callable>(callable));
    }

  private:
    injector_type injector_;
};

} // namespace dingo
