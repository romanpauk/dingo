//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/binding_collection.h>
#include <dingo/core/none.h>
#include <dingo/factory/callable.h>
#include <dingo/registration/type_registration.h>

#include <type_traits>
#include <utility>

namespace dingo {
namespace detail {

template <typename Derived> class runtime_registration_api {
  Derived &self() { return static_cast<Derived &>(*this); }

public:
  decltype(auto) get_allocator() {
    return self().runtime_registry_ref().get_allocator();
  }

  template <typename... TypeArgs> auto &register_type() {
    return self()
        .runtime_registry_ref()
        .template emplace_type_binding<TypeArgs...>(
            self().runtime_registration_parent(), none_t{}, none_t{});
  }

  template <typename... TypeArgs, typename Arg,
            std::enable_if_t<!detail::is_runtime_registration_key_arg_v<Arg>,
                             int> = 0>
  auto &register_type(Arg &&arg) {
    return self()
        .runtime_registry_ref()
        .template emplace_type_binding<TypeArgs...>(
            self().runtime_registration_parent(), std::forward<Arg>(arg),
            none_t{});
  }

  template <typename... TypeArgs, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 (detail::is_runtime_registration_key_arg_v<KeyArgs> && ...)),
                int> = 0>
  auto &register_type(KeyArgs &&...keys) {
    return self()
        .runtime_registry_ref()
        .template emplace_type_binding<TypeArgs...>(
            self().runtime_registration_parent(), none_t{}, none_t{},
            std::forward<KeyArgs>(keys)...);
  }

  template <typename... TypeArgs, typename Arg, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 !detail::is_runtime_registration_key_arg_v<Arg> &&
                 (detail::is_runtime_registration_key_arg_v<KeyArgs> && ...)),
                int> = 0>
  auto &register_type(Arg &&arg, KeyArgs &&...keys) {
    return self()
        .runtime_registry_ref()
        .template emplace_type_binding<TypeArgs...>(
            self().runtime_registration_parent(), std::forward<Arg>(arg),
            none_t{}, std::forward<KeyArgs>(keys)...);
  }

  template <typename... TypeArgs, typename IdType>
  auto &register_indexed_type(IdType &&id) {
    return self()
        .runtime_registry_ref()
        .template emplace_type_binding<TypeArgs...>(
            self().runtime_registration_parent(), none_t{},
            std::forward<IdType>(id));
  }

  template <typename... TypeArgs, typename Arg, typename IdType>
  auto &register_indexed_type(Arg &&arg, IdType &&id) {
    return self()
        .runtime_registry_ref()
        .template emplace_type_binding<TypeArgs...>(
            self().runtime_registration_parent(), std::forward<Arg>(arg),
            std::forward<IdType>(id));
  }

  template <typename... TypeArgs, typename Fn>
  auto &register_type_collection(Fn &&fn) {
    using registration = type_registration<TypeArgs...>;
    return register_type<TypeArgs...>(
        callable([this, collection_fn = std::forward<Fn>(fn)]() mutable {
          return self()
              .template construct_collection<
                  typename registration::storage_type::type>(collection_fn);
        }));
  }

  template <typename... TypeArgs> auto &register_type_collection() {
    return register_type_collection<TypeArgs...>(
        detail::binding_collection_append{});
  }
};

} // namespace detail
} // namespace dingo
