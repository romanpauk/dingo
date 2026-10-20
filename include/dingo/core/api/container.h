//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/container.h of the next implementation.

#include <dingo/core/api/factory/callable.h>    // IWYU pragma: export
#include <dingo/core/api/factory/constructor.h> // IWYU pragma: export
#include <dingo/core/api/factory/function.h>    // IWYU pragma: export
#include <dingo/core/api/registration.h>
#include <dingo/core/backend/runtime_backend.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

#include <utility>

namespace dingo {

template <typename Bindings = bindings<>> class container {
  static_assert(
      core::dependent_false<Bindings>,
      "the next implementation does not provide mixed containers yet");
};

// A container of runtime registrations.
template <> class container<bindings<>> {
public:
  // Registers a binding declared by scope<>, storage<>, and interfaces<>. An
  // external binding takes its instance as the argument.
  template <typename... Parts, typename... Args>
  void register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    backend_.add<binding>(std::forward<Args>(args)...);
  }

  template <typename Request> Request resolve() {
    return backend_.resolve<Request>();
  }

  // Constructs an object of `Type` that no binding stores, by its detected
  // constructor or by the callable factory given, resolving its dependencies
  // from the container.
  template <typename Type> Type construct() {
    return core::detected_factory().make<Type>(backend_);
  }

  template <typename Type, typename Factory> Type construct(Factory &&factory) {
    return factory.template make<Type>(backend_);
  }

private:
  core::runtime_backend backend_;
};

} // namespace dingo
