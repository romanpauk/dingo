//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/container.h of the next implementation.

#include <dingo/core/api/collection.h>
#include <dingo/core/api/container_operations.h>
#include <dingo/core/api/container_traits.h>    // IWYU pragma: export
#include <dingo/core/api/factory/callable.h>    // IWYU pragma: export
#include <dingo/core/api/factory/constructor.h> // IWYU pragma: export
#include <dingo/core/api/factory/function.h>    // IWYU pragma: export
#include <dingo/core/api/key.h>                 // IWYU pragma: export
#include <dingo/core/api/lookup/lookup.h>
#include <dingo/core/api/registration.h>
#include <dingo/core/api/registration_proxy.h>
#include <dingo/core/backend/runtime_backend.h>
#include <dingo/core/backend/selection.h>
#include <dingo/core/backend/static_backend.h>
#include <dingo/core/base/mechanics.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

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

// A container of runtime registrations, with the lookup declarations of its
// traits: container<Traits>, container<Traits, Allocator>, or
// container<Traits, Allocator, Parent>, a child of a parent container that a
// request falls back to. An Allocator or a Parent of void stands for none:
// the allocator of the traits, and a parent of the container's own type.
template <typename Traits = dynamic_container_traits, typename Allocator = void,
          typename Parent = void>
class container
    : public core::selection<container<Traits, Allocator, Parent>>,
      public detail::container_operations<container<Traits, Allocator, Parent>>,
      public detail::container_member_types<Traits> {
public:
  static constexpr bool has_static_source = false;
  static constexpr bool has_runtime_source = true;
  static constexpr bool has_local_static_source = false;
  static constexpr bool has_local_runtime_source = false;
  using parent_type =
      std::conditional_t<std::is_void_v<Parent>, container, Parent>;
  // The allocator of container<Traits, Allocator>, or of its traits, which
  // allocates its bindings.
  using allocator_type =
      std::conditional_t<std::is_void_v<Allocator>,
                         typename detail::traits_allocator<Traits>::type,
                         Allocator>;

  container() : backend_(core::lookup_table_v<Traits>) {}

  explicit container(const allocator_type &allocator)
      : backend_(core::lookup_table_v<Traits>, allocator) {}

  // A child of `parent`, which outlives it.
  explicit container(parent_type *parent,
                     const allocator_type &allocator = allocator_type())
      : backend_(core::lookup_table_v<Traits>, allocator), parent_(parent) {}

  container(const allocator_type &allocator, parent_type *parent)
      : backend_(core::lookup_table_v<Traits>, allocator), parent_(parent) {}

  // Registers a binding declared by scope<>, storage<>, interfaces<>, a key,
  // and a factory. An external binding takes its instance as an argument, a
  // binding of a runtime key its key_value, and a binding of a callable
  // factory the factory.
  // Returns the proxy that registers bindings local to the registration.
  template <typename... Parts, typename... Args>
  auto register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    auto &locals = backend_.template add<binding>(std::forward<Args>(args)...);
    return core::registration_proxy<std::remove_reference_t<decltype(locals)>>(
        locals, core::lookup_table_v<Traits>);
  }

  // The sources of the selection of the container, which resolves the
  // dependencies of their bindings, and keeps the objects requests construct.
  auto &container_resolver() { return *this; }
  core::retained_objects &retained() { return retained_; }
  core::runtime_backend<container, allocator_type> &runtime_source() {
    return backend_;
  }
  parent_type *parent() { return parent_; }
  core::transaction_log &transactions() { return backend_.transactions(); }

private:
  core::retained_objects retained_;
  core::runtime_backend<container, allocator_type> backend_;
  parent_type *parent_ = nullptr;
};

// A container of the static bindings of its type and of runtime
// registrations, which share its lookup declarations: a request selects among
// both, and a runtime registration whose key a static binding holds is
// rejected. It takes the parent, when it has one, and the instance of every
// external static binding as constructor arguments, in the order of the
// bindings.
template <typename... Binds, typename Second, typename Third>
class container<bindings<Binds...>, Second, Third>
    : public core::selection<container<bindings<Binds...>, Second, Third>>,
      public detail::container_operations<
          container<bindings<Binds...>, Second, Third>>,
      public detail::container_member_types<
          typename detail::mixed_parameters<Second, Third>::traits> {
  using parameters = detail::mixed_parameters<Second, Third>;
  using lookups = core::lookup_table_of<typename core::lookup_list<
      typename parameters::traits::lookup_definition_type>::type>;

public:
  using static_source_type = core::static_backend<
      lookups, typename core::bind_registration<Binds>::type::binding...>;
  static constexpr bool has_static_source = true;
  static constexpr bool has_runtime_source = true;
  static constexpr bool has_local_static_source = false;
  static constexpr bool has_local_runtime_source = false;
  using parent_type = typename parameters::parent;
  using allocator_type =
      typename detail::traits_allocator<typename parameters::traits>::type;

  template <typename... Instances, bool Orphan = std::is_void_v<parent_type>,
            typename = std::enable_if_t<Orphan>>
  explicit container(Instances &&...instances)
      : static_(std::forward<Instances>(instances)...),
        runtime_(lookups::value) {}

  // A child of `parent`, which outlives it.
  template <typename... Instances, bool Child = !std::is_void_v<parent_type>,
            typename = std::enable_if_t<Child>>
  explicit container(std::conditional_t<Child, parent_type, container> *parent,
                     Instances &&...instances)
      : static_(std::forward<Instances>(instances)...),
        runtime_(lookups::value), parent_(parent) {}

  // Registers a runtime binding, as container<Traits> does.
  template <typename... Parts, typename... Args>
  auto register_type(Args &&...args) {
    using binding =
        typename core::registration<Parts...>::template binding_for<Args...>;
    auto &locals = runtime_.template add_unless<binding>(
        [](const void *key_value) {
          return static_source_type::template holds_key_of<binding>(key_value);
        },
        std::forward<Args>(args)...);
    return core::registration_proxy<std::remove_reference_t<decltype(locals)>>(
        locals, lookups::value);
  }

  // The sources of the selection of the container, which resolves the
  // dependencies of their bindings, and keeps the objects requests construct.
  auto &container_resolver() { return *this; }
  core::retained_objects &retained() { return retained_; }
  static_source_type &static_source() { return static_; }
  core::runtime_backend<container, allocator_type> &runtime_source() {
    return runtime_;
  }
  parent_type *parent() { return parent_; }
  core::transaction_log &transactions() { return runtime_.transactions(); }

private:
  core::retained_objects retained_;
  static_source_type static_;
  core::runtime_backend<container, allocator_type> runtime_;
  parent_type *parent_ = nullptr;
};

} // namespace dingo
