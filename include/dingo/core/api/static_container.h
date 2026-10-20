//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/static_container.h of the next implementation.

#include <dingo/core/api/collection.h>
#include <dingo/core/api/container_operations.h>
#include <dingo/core/api/container_traits.h>    // IWYU pragma: export
#include <dingo/core/api/factory/callable.h>    // IWYU pragma: export
#include <dingo/core/api/factory/constructor.h> // IWYU pragma: export
#include <dingo/core/api/factory/function.h>    // IWYU pragma: export
#include <dingo/core/api/key.h>                 // IWYU pragma: export
#include <dingo/core/api/lookup/lookup.h>
#include <dingo/core/api/registration.h>
#include <dingo/core/backend/selection.h>
#include <dingo/core/backend/static_backend.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Type> using is_container_traits = is_traits_type<Type>;

// The parent of a container, nothing without one.
template <typename Parent> struct parent_holder {
  Parent *parent_pointer = nullptr;
};

template <> struct parent_holder<void> {};
} // namespace detail

template <typename Bindings, typename Second = static_container_traits>
class static_container;

// A container of the bindings of its type: static_container<bindings<...>>,
// with the lookup declarations of traits static_container<bindings<...>,
// Traits>, or a child of a parent container static_container<bindings<...>,
// Parent>. It takes the parent, when it has one, and the instance of every
// external binding as constructor arguments, in the order of the bindings.
template <typename... Binds, typename Second>
class DINGO_EMPTY_BASES static_container<bindings<Binds...>, Second>
    : public core::selection<static_container<bindings<Binds...>, Second>>,
      public detail::container_operations<
          static_container<bindings<Binds...>, Second>>,
      public detail::container_member_types<
          std::conditional_t<detail::is_container_traits<Second>::value, Second,
                             static_container_traits>>,
      private detail::parent_holder<std::conditional_t<
          detail::is_container_traits<Second>::value, void, Second>> {
  using traits = std::conditional_t<detail::is_container_traits<Second>::value,
                                    Second, static_container_traits>;

public:
  using static_source_type = core::static_backend<
      core::lookup_table_of<typename core::lookup_list<
          typename traits::lookup_definition_type>::type>,
      typename core::bind_registration<Binds>::type::binding...>;
  static constexpr bool has_static_source = true;
  static constexpr bool has_runtime_source = false;
  static constexpr bool has_local_static_source = false;
  static constexpr bool has_local_runtime_source = false;
  using parent_type =
      std::conditional_t<detail::is_container_traits<Second>::value, void,
                         Second>;

  template <typename... Instances, bool Orphan = std::is_void_v<parent_type>,
            typename = std::enable_if_t<Orphan>>
  explicit static_container(Instances &&...instances)
      : backend_(std::forward<Instances>(instances)...) {}

  // A child of `parent`, which outlives it.
  template <typename... Instances, bool Child = !std::is_void_v<parent_type>,
            typename = std::enable_if_t<Child>>
  explicit static_container(
      std::conditional_t<Child, parent_type, static_container> *parent,
      Instances &&...instances)
      : backend_(std::forward<Instances>(instances)...) {
    this->parent_pointer = parent;
  }

  // The sources of the selection of the container, which resolves the
  // dependencies of their bindings, and keeps the objects requests construct.
  auto &container_resolver() { return *this; }
  core::retained_objects &retained() { return retained_; }
  static_source_type &static_source() { return backend_; }
  auto &transactions() { return backend_.transactions(); }

  template <typename Parent = parent_type> Parent *parent() {
    if constexpr (std::is_void_v<Parent>) {
      return nullptr;
    } else {
      return this->parent_pointer;
    }
  }

private:
  core::retained_objects retained_;
  static_source_type backend_;
};

} // namespace dingo
