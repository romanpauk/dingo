//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/container.h of the next implementation.

#include <dingo/core/api/collection.h>
#include <dingo/core/api/container_traits.h>    // IWYU pragma: export
#include <dingo/core/api/factory/callable.h>    // IWYU pragma: export
#include <dingo/core/api/factory/constructor.h> // IWYU pragma: export
#include <dingo/core/api/factory/function.h>    // IWYU pragma: export
#include <dingo/core/api/key.h>                 // IWYU pragma: export
#include <dingo/core/api/lookup/lookup.h>
#include <dingo/core/api/registration.h>
#include <dingo/core/backend/runtime_backend.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Type> struct is_bindings : std::false_type {};

template <typename... Binds>
struct is_bindings<bindings<Binds...>> : std::true_type {};
} // namespace detail

// A container of runtime registrations, with the lookup declarations of its
// traits.
template <typename Traits = dynamic_container_traits, typename... Rest>
class container {
  static_assert(
      !detail::is_bindings<Traits>::value,
      "the next implementation does not provide mixed containers yet");
  static_assert(sizeof...(Rest) == 0,
                "the next implementation does not provide container allocators "
                "and parents yet");

public:
  container() : backend_(core::lookup_table_v<Traits>) {}

  // Registers a binding declared by scope<>, storage<>, interfaces<>, a key,
  // and a factory. An external binding takes its instance as an argument, a
  // binding of a runtime key its key_value, and a binding of a callable
  // factory the factory.
  template <typename... Parts, typename... Args>
  void register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    backend_.add<binding>(std::forward<Args>(args)...);
  }

  // Registers a binding that stores a collection of the bindings of its
  // element type, each added by `append`, or by its collection traits.
  template <typename... Parts, typename... Append>
  void register_type_collection(Append &&...append) {
    static_assert(sizeof...(Append) <= 1,
                  "a collection takes at most one append function");
    if constexpr (sizeof...(Append) == 0) {
      register_type<Parts...>(core::collection_factory<core::collection_append>(
          core::collection_append()));
    } else {
      register_type<Parts...>(core::collection_factory<std::decay_t<Append>...>(
          std::forward<Append>(append)...));
    }
  }

  template <typename Request> Request resolve() {
    return backend_.resolve<Request>();
  }

  // Resolves a request by a key: a key_type object, a key_value, or a value
  // of the key type.
  template <typename Request, typename Value> Request resolve(Value &&value) {
    return backend_.resolve<Request>(std::forward<Value>(value));
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
  core::runtime_backend backend_;
};

} // namespace dingo
