//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/container.h of the next implementation.

#include <dingo/collection.h>
#include <dingo/container_operations.h>
#include <dingo/container_traits.h> // IWYU pragma: export
#include <dingo/core/backend/runtime_backend.h>
#include <dingo/core/backend/selection.h>
#include <dingo/core/backend/static_backend.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/allocation.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export
#include <dingo/factory/callable.h>           // IWYU pragma: export
#include <dingo/factory/constructor.h>        // IWYU pragma: export
#include <dingo/factory/function.h>           // IWYU pragma: export
#include <dingo/key.h>                        // IWYU pragma: export
#include <dingo/lookup/lookup.h>
#include <dingo/registration.h>
#include <dingo/registration_proxy.h>

#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Type> struct is_bindings : std::false_type {};

template <typename... Binds>
struct is_bindings<bindings<Binds...>> : std::true_type {};

template <typename Type> using is_traits = is_traits_type<Type>;

// The traits and the parent of a mixed container: container<bindings<...>>,
// container<bindings<...>, Traits>, container<bindings<...>, Parent>, or
// container<bindings<...>, Traits, Parent>; void stands for none.
template <typename Second, typename Third> struct mixed_parameters {
  using traits = Second;
  using parent = Third;
};

template <typename Second> struct mixed_parameters<Second, void> {
  using traits = std::conditional_t<is_traits<Second>::value, Second,
                                    dynamic_container_traits>;
  using parent = std::conditional_t<is_traits<Second>::value, void, Second>;
};

template <> struct mixed_parameters<void, void> {
  using traits = dynamic_container_traits;
  using parent = void;
};

} // namespace detail

template <typename Traits = dynamic_container_traits, typename Allocator = void,
          typename Parent = void>
class container;

namespace detail {
// The parent type of container<Traits, Allocator, Parent>: Parent, or a
// container of the same type.
template <typename Container, typename Parent>
using parent_or_self_t =
    std::conditional_t<std::is_void_v<Parent>, Container, Parent>;

// The allocator of container<Traits, Allocator>, or of its traits.
template <typename Traits, typename Allocator>
using runtime_allocator_t =
    std::conditional_t<std::is_void_v<Allocator>,
                       typename traits_allocator<Traits>::type, Allocator>;

// The state of a runtime container: its bindings, the objects it keeps for
// requests, and its parent.
template <typename Container, typename Traits, typename Allocator,
          typename Parent>
struct runtime_state {
  runtime_state(const Allocator &allocator, Parent *parent_container)
      : backend(core::lookup_table_v<Traits>, allocator),
        parent(parent_container) {}

  core::retained_objects retained;
  core::runtime_backend<Container, Allocator> backend;
  parent_link<Parent> parent;
  void *owner = nullptr;
};

// The state of a mixed container: its static and runtime bindings, the
// objects it keeps for requests, and its parent.
template <typename Container, typename Lookups, typename StaticSource,
          typename Allocator, typename Parent>
struct mixed_state {
  template <typename... Instances>
  explicit mixed_state(Parent *parent_container, Instances &&...instances)
      : static_source(std::forward<Instances>(instances)...),
        runtime(Lookups::value), parent(parent_container) {}

  core::retained_objects retained;
  StaticSource static_source;
  core::runtime_backend<Container, Allocator> runtime;
  parent_link<Parent> parent;
  void *owner = nullptr;
};

// Whether a registration of `Binding` into a container of `Traits` by `Args`
// takes a key value for every lookup of an interface by a runtime key its
// traits declare, when it takes one for any.
template <typename Traits>
using declarations_of =
    typename core::lookup_list<typename Traits::lookup_definition_type>::type;

template <typename Traits, typename Binding, typename... Args>
constexpr bool key_values_complete() {
  return core::detail::key_values_complete<declarations_of<Traits>, Binding,
                                           Args...>();
}

template <typename Traits, typename Allocator, typename Parent>
struct runtime_parameters {
  using container = ::dingo::container<Traits, Allocator, Parent>;
  using parent = parent_or_self_t<container, Parent>;
  using allocator = runtime_allocator_t<Traits, Allocator>;
  using state =
      container_state<runtime_state<container, Traits, allocator, parent>,
                      allocator, true>;
};

template <typename Second, typename Third, typename... Binds>
struct mixed_container_parameters {
  using container = ::dingo::container<bindings<Binds...>, Second, Third>;
  using parameters = mixed_parameters<Second, Third>;
  using lookups = core::lookup_table_of<typename core::lookup_list<
      typename parameters::traits::lookup_definition_type>::type>;
  using static_source = core::static_backend<
      lookups, typename core::bind_registration<Binds>::type::binding...>;
  using parent = typename parameters::parent;
  using allocator =
      typename traits_allocator<typename parameters::traits>::type;
  // A container without static bindings keeps its state apart from itself
  // and takes it along when it moves, as a runtime container does; one with
  // static bindings keeps them in place, and does not move.
  using state = container_state<
      mixed_state<container, lookups, static_source, allocator, parent>,
      allocator, sizeof...(Binds) == 0>;
};
} // namespace detail

// A container of runtime registrations, with the lookup declarations of its
// traits: container<Traits>, container<Traits, Allocator>, or
// container<Traits, Allocator, Parent>, a child of a parent container that a
// request falls back to. An Allocator or a Parent of void stands for none:
// the allocator of the traits, and a parent of the container's own type. It
// keeps its state apart from itself and takes it along when it moves, so its
// bindings, the objects they keep, the proxies of its registrations, and its
// children stay valid; a moved-from container can only be destroyed or
// assigned to.
template <typename Traits, typename Allocator, typename Parent>
class container
    : public core::selection<container<Traits, Allocator, Parent>>,
      public detail::container_operations<container<Traits, Allocator, Parent>>,
      public detail::container_member_types<Traits>,
      public detail::runtime_parameters<Traits, Allocator, Parent>::state {
  using parameters = detail::runtime_parameters<Traits, Allocator, Parent>;

public:
  static constexpr bool has_static_source = false;
  static constexpr bool has_runtime_source = true;
  static constexpr bool has_local_static_source = false;
  static constexpr bool has_local_runtime_source = false;
  using parent_type = typename parameters::parent;
  // The allocator of container<Traits, Allocator>, or of its traits, which
  // allocates its bindings and its state.
  using allocator_type = typename parameters::allocator;
  using state_holder = typename parameters::state;

  container() : container(allocator_type(), nullptr) {}

  explicit container(const allocator_type &allocator)
      : container(allocator, nullptr) {}

  // A child of `parent`, which outlives it.
  explicit container(parent_type *parent,
                     const allocator_type &allocator = allocator_type())
      : container(allocator, parent) {}

  container(const allocator_type &allocator, parent_type *parent)
      : state_holder(allocator, allocator, parent) {}

  container(container &&) noexcept = default;
  container &operator=(container &&) noexcept = default;

  // Registers a binding declared by scope<>, storage<>, interfaces<>, a key,
  // and a factory. An external binding takes its instance as an argument, a
  // binding of a runtime key its key_value, and a binding of a callable
  // factory the factory.
  // Returns the proxy that registers bindings local to the registration.
  template <typename... Parts, typename... Args>
  auto register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    static_assert(detail::key_values_complete<Traits, binding, Args...>());
    auto &locals = this->state()->backend.template add<binding>(
        std::forward<Args>(args)...);
    return core::registration_proxy<std::remove_reference_t<decltype(locals)>,
                                    detail::declarations_of<Traits>>(
        locals, core::lookup_table_v<Traits>);
  }

  // The sources of the selection of the container, which resolves the
  // dependencies of their bindings, and keeps the objects requests construct.
  auto &container_resolver() { return *this; }
  core::retained_objects &retained() { return this->state()->retained; }
  core::runtime_backend<container, allocator_type> &runtime_source() {
    return this->state()->backend;
  }
  parent_type *parent() { return this->state()->parent.get(); }
  core::transaction_log &transactions() {
    return this->state()->backend.transactions();
  }
};

// A container of the static bindings of its type and of runtime
// registrations, which share its lookup declarations: a request selects among
// both, and a runtime registration whose key a static binding holds is
// rejected. It takes the parent, when it has one, and the instance of every
// external static binding as constructor arguments, in the order of the
// bindings. Without static bindings it moves as a runtime container does;
// with them it keeps them in place, and does not move.
template <typename... Binds, typename Second, typename Third>
class container<bindings<Binds...>, Second, Third>
    : public core::selection<container<bindings<Binds...>, Second, Third>>,
      public detail::container_operations<
          container<bindings<Binds...>, Second, Third>>,
      public detail::container_member_types<
          typename detail::mixed_parameters<Second, Third>::traits>,
      public detail::mixed_container_parameters<Second, Third,
                                                Binds...>::state {
  using parameters =
      detail::mixed_container_parameters<Second, Third, Binds...>;
  using lookups = typename parameters::lookups;

public:
  using static_source_type = typename parameters::static_source;
  static constexpr bool has_static_source = true;
  static constexpr bool has_runtime_source = true;
  static constexpr bool has_local_static_source = false;
  static constexpr bool has_local_runtime_source = false;
  using parent_type = typename parameters::parent;
  using allocator_type = typename parameters::allocator;
  using state_holder = typename parameters::state;

  template <typename... Instances, bool Orphan = std::is_void_v<parent_type>,
            typename = std::enable_if_t<
                Orphan && !detail::is_self<container, Instances...>::value>>
  explicit container(Instances &&...instances)
      : state_holder(allocator_type(), nullptr,
                     std::forward<Instances>(instances)...) {}

  // A child of `parent`, which outlives it.
  template <typename... Instances, bool Child = !std::is_void_v<parent_type>,
            typename = std::enable_if_t<Child>>
  explicit container(std::conditional_t<Child, parent_type, container> *parent,
                     Instances &&...instances)
      : state_holder(allocator_type(), parent,
                     std::forward<Instances>(instances)...) {}

  container(container &&) noexcept = default;
  container &operator=(container &&) noexcept = default;

  // Registers a runtime binding, as container<Traits> does.
  template <typename... Parts, typename... Args>
  auto register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    static_assert(detail::key_values_complete<
                  typename detail::mixed_parameters<Second, Third>::traits,
                  binding, Args...>());
    auto &locals = this->state()->runtime.template add_unless<binding>(
        [](const void *key_value) {
          return static_source_type::template holds_key_of<binding>(key_value);
        },
        std::forward<Args>(args)...);
    return core::registration_proxy<
        std::remove_reference_t<decltype(locals)>,
        detail::declarations_of<
            typename detail::mixed_parameters<Second, Third>::traits>>(
        locals, lookups::value);
  }

  // The sources of the selection of the container, which resolves the
  // dependencies of their bindings, and keeps the objects requests construct.
  auto &container_resolver() { return *this; }
  core::retained_objects &retained() { return this->state()->retained; }
  static_source_type &static_source() { return this->state()->static_source; }
  core::runtime_backend<container, allocator_type> &runtime_source() {
    return this->state()->runtime;
  }
  parent_type *parent() { return this->state()->parent.get(); }
  core::transaction_log &transactions() {
    return this->state()->runtime.transactions();
  }
};

} // namespace dingo
