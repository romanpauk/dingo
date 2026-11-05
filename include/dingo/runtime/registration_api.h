//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/base/none.h>
#include <dingo/binding/binding_collection.h>
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

  // Registers TypeArgs, built by an optional factory argument and found by
  // the runtime key values that follow it.
  template <typename... TypeArgs, typename... Args>
  auto register_type(Args &&...args) {
    if constexpr (are_runtime_registration_key_args_v<Args...>) {
      return self().binding_store().template prepare_binding<TypeArgs...>(
          &self().runtime_registration_parent(), none_t{},
          std::forward<Args>(args)...);
    } else {
      return register_with_argument<TypeArgs...>(std::forward<Args>(args)...);
    }
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

private:
  template <typename... TypeArgs, typename Arg, typename... KeyArgs>
  auto register_with_argument(Arg &&arg, KeyArgs &&...keys) {
    static_assert(are_runtime_registration_key_args_v<KeyArgs...>,
                  "register_type takes a factory argument followed by "
                  "runtime key values");
    return self().binding_store().template prepare_binding<TypeArgs...>(
        &self().runtime_registration_parent(), std::forward<Arg>(arg),
        std::forward<KeyArgs>(keys)...);
  }
};

} // namespace detail
} // namespace dingo
