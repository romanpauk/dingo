//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/static_container.h of the next implementation.

#include <dingo/core/api/collection.h>
#include <dingo/core/api/container_traits.h>    // IWYU pragma: export
#include <dingo/core/api/factory/callable.h>    // IWYU pragma: export
#include <dingo/core/api/factory/constructor.h> // IWYU pragma: export
#include <dingo/core/api/factory/function.h>    // IWYU pragma: export
#include <dingo/core/api/key.h>                 // IWYU pragma: export
#include <dingo/core/api/lookup/lookup.h>
#include <dingo/core/api/registration.h>
#include <dingo/core/backend/static_backend.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

#include <utility>

namespace dingo {

template <typename Bindings, typename Traits = static_container_traits>
class static_container;

// A container of the bindings of its type. It takes the instance of every
// external binding as a constructor argument, in the order of the bindings.
template <typename... Binds, typename Traits>
class static_container<bindings<Binds...>, Traits> {
public:
  template <typename... Instances>
  explicit static_container(Instances &&...instances)
      : backend_(std::forward<Instances>(instances)...) {}

  template <typename Request> Request resolve() {
    return backend_.template resolve<Request>();
  }

  // Resolves a request by a key value: a key_value, or a value of the key
  // type.
  template <typename Request, typename Value> Request resolve(Value &&value) {
    return backend_.template resolve<Request>(std::forward<Value>(value));
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

  // Constructs a collection of every binding of its element type, by a key
  // and an append function, either of which may be given.
  template <typename Collection, typename... Arguments>
  Collection construct_collection(Arguments &&...arguments) {
    return core::collect_by<Collection>(backend_,
                                        std::forward<Arguments>(arguments)...);
  }

private:
  core::static_backend<
      core::lookup_table_of<typename core::lookup_list<
          typename Traits::lookup_definition_type>::type>,
      typename core::bind_registration<Binds>::type::binding...>
      backend_;
};

} // namespace dingo
