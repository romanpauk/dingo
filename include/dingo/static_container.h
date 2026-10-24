//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/static_container.h of the next implementation.

#include <dingo/backend/selection.h>
#include <dingo/backend/static_backend.h>
#include <dingo/collection.h>
#include <dingo/container_operations.h>
#include <dingo/container_traits.h>       // IWYU pragma: export
#include <dingo/diagnostics/exceptions.h> // IWYU pragma: export
#include <dingo/factory/callable.h>       // IWYU pragma: export
#include <dingo/factory/constructor.h>    // IWYU pragma: export
#include <dingo/factory/function.h>       // IWYU pragma: export
#include <dingo/key.h>                    // IWYU pragma: export
#include <dingo/lookup/lookup.h>
#include <dingo/registration.h>

#include <type_traits>
#include <utility>

namespace dingo {

namespace detail {
template <typename Type> using is_container_traits = is_traits_type<Type>;

// The parent of a container, nothing without one.
template <typename Parent> struct parent_holder {
  parent_link<Parent> parent_pointer;
};

template <> struct parent_holder<void> {};

// The traits of a static container whose second argument is `Second`: the
// traits it names, the traits of the parent it names, as the baseline takes
// them, or the default traits of a static container.
template <typename Second, typename = void> struct parent_traits {
  using type = static_container_traits;
};

template <typename Second>
struct parent_traits<Second, std::void_t<typename Second::traits_type>> {
  using type = typename Second::traits_type;
};

template <typename Second>
using static_traits_t =
    typename std::conditional_t<is_container_traits<Second>::value,
                                std::enable_if<true, Second>,
                                parent_traits<Second>>::type;
} // namespace detail

template <typename Bindings, typename Second = static_container_traits>
class static_container;

// A container of the bindings of its type: static_container<bindings<...>>,
// with the lookup declarations of traits static_container<bindings<...>,
// Traits>, or a child of a parent container static_container<bindings<...>,
// Parent>, which has the traits of its parent. It takes the parent, when it
// has one, and the instance of every external binding as constructor
// arguments, in the order of the bindings.
template <typename... Binds, typename Second>
class DINGO_EMPTY_BASES static_container<bindings<Binds...>, Second>
    : public core::selection<static_container<bindings<Binds...>, Second>>,
      public detail::container_operations<
          static_container<bindings<Binds...>, Second>>,
      public detail::container_member_types<detail::static_traits_t<Second>>,
      private detail::parent_holder<std::conditional_t<
          detail::is_container_traits<Second>::value, void, Second>> {
  using traits = detail::static_traits_t<Second>;

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

  template <
      typename... Instances, bool Orphan = std::is_void_v<parent_type>,
      typename = std::enable_if_t<
          Orphan && !detail::is_self<static_container, Instances...>::value>>
  explicit static_container(Instances &&...instances)
      : backend_(std::forward<Instances>(instances)...) {
    static_assert(static_container::declared_dependencies_named(
        typename static_source_type::binding_list()));
  }

  // A child of `parent`, which outlives it.
  template <typename... Instances, bool Child = !std::is_void_v<parent_type>,
            typename = std::enable_if_t<Child>>
  explicit static_container(
      std::conditional_t<Child, parent_type, static_container> *parent,
      Instances &&...instances)
      : backend_(std::forward<Instances>(instances)...) {
    static_assert(static_container::declared_dependencies_named(
        typename static_source_type::binding_list()));
    this->parent_pointer = detail::parent_link<parent_type>(parent);
  }

  // Its bindings are kept in place, so it does not move. The destructor is
  // inlined with the requests, which destroy the container when they unwind.
  static_container(const static_container &) = delete;
  static_container &operator=(const static_container &) = delete;
  DINGO_ALWAYS_INLINE ~static_container() = default;

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
      return this->parent_pointer.get();
    }
  }

private:
  core::retained_objects retained_;
  static_source_type backend_;
};

} // namespace dingo
