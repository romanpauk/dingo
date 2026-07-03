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
    return self().binding_store().get_allocator();
  }

  template <typename... TypeArgs> auto register_type() {
    return self().binding_store().template prepare_binding<TypeArgs...>(
        &self().runtime_registration_parent(), none_t{});
  }

  template <typename... TypeArgs, typename Arg,
            std::enable_if_t<!detail::is_runtime_registration_key_arg_v<Arg>,
                             int> = 0>
  auto register_type(Arg &&arg) {
    return self().binding_store().template prepare_binding<TypeArgs...>(
        &self().runtime_registration_parent(), std::forward<Arg>(arg));
  }

  template <typename... TypeArgs, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 detail::are_runtime_registration_key_args_v<KeyArgs...>),
                int> = 0>
  auto register_type(KeyArgs &&...keys) {
    return self().binding_store().template prepare_binding<TypeArgs...>(
        &self().runtime_registration_parent(), none_t{},
        std::forward<KeyArgs>(keys)...);
  }

  template <typename... TypeArgs, typename Arg, typename... KeyArgs,
            std::enable_if_t<
                (sizeof...(KeyArgs) > 0 &&
                 !detail::is_runtime_registration_key_arg_v<Arg> &&
                 detail::are_runtime_registration_key_args_v<KeyArgs...>),
                int> = 0>
  auto register_type(Arg &&arg, KeyArgs &&...keys) {
    return self().binding_store().template prepare_binding<TypeArgs...>(
        &self().runtime_registration_parent(), std::forward<Arg>(arg),
        std::forward<KeyArgs>(keys)...);
  }

  template <typename... TypeArgs, typename Fn>
  auto register_type_collection(Fn &&fn) {
    using registration = type_registration<TypeArgs...>;
    return register_type<TypeArgs...>(
        callable([this, collection_fn = std::forward<Fn>(fn)]() mutable {
          return self()
              .template construct_collection<
                  typename registration::storage_type::type>(collection_fn);
        }));
  }

  template <typename... TypeArgs> auto register_type_collection() {
    return register_type_collection<TypeArgs...>(
        detail::binding_collection_append{});
  }
};

} // namespace detail
} // namespace dingo
