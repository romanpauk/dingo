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
#include <dingo/core/backend/selection.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Type> struct is_bindings : std::false_type {};

template <typename... Binds>
struct is_bindings<bindings<Binds...>> : std::true_type {};

template <typename... Rest> struct runtime_parent {
  using type = void;
};

template <typename Allocator, typename Parent>
struct runtime_parent<Allocator, Parent> {
  using type = Parent;
};
} // namespace detail

// A container of runtime registrations, with the lookup declarations of its
// traits: container<Traits>, container<Traits, Allocator>, or
// container<Traits, Allocator, Parent>, a child of a parent container that a
// request falls back to.
template <typename Traits = dynamic_container_traits, typename... Rest>
class container : public core::selection<container<Traits, Rest...>> {
  static_assert(
      !detail::is_bindings<Traits>::value,
      "the next implementation does not provide mixed containers yet");
  static_assert(sizeof...(Rest) <= 2,
                "container<Traits, Allocator, Parent> takes at most three "
                "parameters");

public:
  static constexpr bool has_static_source = false;
  static constexpr bool has_runtime_source = true;
  using parent_type = typename detail::runtime_parent<Rest...>::type;

  container() : backend_(core::lookup_table_v<Traits>) {}

  // A child of `parent`, which outlives it.
  template <bool Child = !std::is_void_v<parent_type>,
            typename = std::enable_if_t<Child>>
  explicit container(std::conditional_t<Child, parent_type, container> *parent)
      : backend_(core::lookup_table_v<Traits>), parent_(parent) {}

  // Registers a binding declared by scope<>, storage<>, interfaces<>, a key,
  // and a factory. An external binding takes its instance as an argument, a
  // binding of a runtime key its key_value, and a binding of a callable
  // factory the factory.
  template <typename... Parts, typename... Args>
  void register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    backend_.template add<binding>(std::forward<Args>(args)...);
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

  // Constructs an object of `Type` that no binding stores, by its detected
  // constructor or by the callable factory given, resolving its dependencies
  // from the container.
  template <typename Type> Type construct() {
    return core::detected_factory().make<Type>(*this);
  }

  template <typename Type, typename Factory> Type construct(Factory &&factory) {
    return factory.template make<Type>(*this);
  }

  // Constructs a collection of every binding of its element type, by a key
  // and an append function, either of which may be given.
  template <typename Collection, typename... Arguments>
  Collection construct_collection(Arguments &&...arguments) {
    return core::collect_by<Collection>(*this,
                                        std::forward<Arguments>(arguments)...);
  }

  // The sources of the selection of the container.
  core::runtime_backend<container> &runtime_source() { return backend_; }
  parent_type *parent() { return parent_; }
  core::publication_log &publications() { return backend_.publications(); }

private:
  core::runtime_backend<container> backend_;
  parent_type *parent_ = nullptr;
};

} // namespace dingo
