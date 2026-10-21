//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The operations every container of the next implementation provides on its
// selection.

#include <dingo/collection.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/type_path.h>
#include <dingo/core/projection/construct.h>
#include <dingo/core/vocabulary/compile_diagnostics.gen.h>
#include <dingo/factory/callable.h>
#include <dingo/registration.h>

#include <functional>
#include <type_traits>
#include <utility>

namespace dingo::detail {

// The operations of a container on its selection: constructing objects and
// collections no binding stores, invoking a callable with its arguments
// resolved, and registering a binding that stores a collection.
template <typename Self> class container_operations {
public:
  // Registers a binding that stores a collection of the bindings of its
  // element type: by the key, or each added by the append function, of its
  // argument, or by its collection traits.
  template <typename... Parts, typename... Append>
  void register_type_collection(Append &&...append) {
    static_assert(core::require<sizeof...(Append) <= 1,
                                core::diag::collection_arguments>());
    if constexpr (sizeof...(Append) == 0) {
      self().template register_type<Parts...>(
          core::collection_factory<core::collection_append>(
              core::collection_append()));
    } else {
      self().template register_type<Parts...>(
          core::collection_factory<std::decay_t<Append>...>(
              std::forward<Append>(append)...));
    }
  }

  // Constructs an object of `Type`: a binding of the type answers the request
  // as resolve does, and otherwise its detected constructor makes it,
  // resolving its dependencies from the container. `Factory`, when given, is
  // a factory of the public API, such as constructor<Leaf>, that makes the
  // object, and so does the callable factory given.
  template <typename Type, typename Factory = void> Type construct() {
    if constexpr (std::is_void_v<Factory>) {
      return self().template construct_by_rule<Type>();
    } else {
      return self().transactions().run([&]() -> Type {
        return typename core::factory_of<Factory>::type().template make<Type>(
            self());
      });
    }
  }

  template <typename Type, typename Factory> Type construct(Factory &&factory) {
    return self().transactions().run(
        [&]() -> Type { return factory.template make<Type>(self()); });
  }

  // Constructs a collection of every binding of its element type, by a key
  // and an append function, either of which may be given.
  template <typename Collection, typename... Arguments>
  Collection construct_collection(Arguments &&...arguments) {
    return core::collect_by<Collection>(self(),
                                        std::forward<Arguments>(arguments)...);
  }

  // Calls `callable` with its arguments resolved from the container;
  // `Signature`, when given, names the argument types of a callable whose
  // call operator is overloaded or a template.
  template <typename Signature = void, typename Callable>
  decltype(auto) invoke(Callable &&callable) {
    using arguments = typename ::dingo::detail::callable_arguments<
        Signature, std::decay_t<Callable>>::type;
    const auto frame = core::type_frame::of<std::decay_t<Callable>>();
    return self().transactions().run([&]() -> decltype(auto) {
      return invoke_with(std::forward<Callable>(callable), arguments());
    });
  }

private:
  // Calls the callable in its value category, so a call operator that takes
  // its object as an rvalue applies.
  template <typename Callable, typename... Args>
  decltype(auto) invoke_with(Callable &&callable,
                             core::type_list<Args...> /*arguments*/) {
    return std::invoke(std::forward<Callable>(callable),
                       self().template argument<Args>()...);
  }

  Self &self() { return static_cast<Self &>(*this); }
};
} // namespace dingo::detail
