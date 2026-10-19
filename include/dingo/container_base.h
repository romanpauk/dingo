//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/binding_selection.h>
#include <dingo/lookup/container_lookup_definition.h>
#include <dingo/registration/type_registration.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/registry.h>
#include <dingo/static/container_traits.h>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

// The runtime-only facade: the resolve front-end with empty static bindings
// and a runtime-only configuration. Registered bindings, registration
// containers and child containers point back at the front-end, so it lives at
// a stable heap address and runtime_container moves by transferring ownership.
template <typename ContainerTraits = dynamic_container_traits,
          typename Allocator = typename ContainerTraits::allocator_type,
          typename ParentContainer = void>
class runtime_container {
public:
  using implementation_type = detail::container_with_static_bindings<
      static_bindings<>, ParentContainer,
      detail::static_container_runtime_config<ContainerTraits, Allocator, void,
                                              void, true>>;

private:
  using implementation_allocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<implementation_type>;
  using implementation_allocator_traits =
      std::allocator_traits<implementation_allocator>;

  template <typename, typename> friend struct detail::container_implementation;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
  using registry_type = typename implementation_type::registry_type;
  using parent_container_type =
      std::conditional_t<std::is_same_v<void, ParentContainer>,
                         runtime_container, ParentContainer>;
  using rtti_type = typename implementation_type::rtti_type;
  using lookup_definition_type =
      typename implementation_type::lookup_definition_type;

  runtime_container() : runtime_container(allocator_type()) {}

  explicit runtime_container(const allocator_type &alloc)
      : impl_(create(alloc, alloc)) {}

  runtime_container(parent_container_type *parent,
                    const allocator_type &alloc = allocator_type())
      : impl_(create(alloc, detail::container_implementation_of(parent),
                     alloc)) {}

  // Moving transfers the container state, so registrations and resolved
  // instances keep their addresses. The moved-from container can only be
  // destroyed or assigned to.
  runtime_container(runtime_container &&other) noexcept
      : impl_(std::exchange(other.impl_, nullptr)) {}

  runtime_container &operator=(runtime_container &&other) noexcept {
    if (this != &other) {
      destroy();
      impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
  }

  runtime_container(const runtime_container &) = delete;
  runtime_container &operator=(const runtime_container &) = delete;

  ~runtime_container() { destroy(); }

  decltype(auto) get_allocator() { return implementation().get_allocator(); }

  template <typename... TypeArgs, typename... Args>
  auto register_type(Args &&...args) {
    return implementation().template register_type<TypeArgs...>(
        std::forward<Args>(args)...);
  }

  template <typename... TypeArgs, typename... Args>
  auto register_type_collection(Args &&...args) {
    return implementation().template register_type_collection<TypeArgs...>(
        std::forward<Args>(args)...);
  }

  template <typename... TypeArgs, typename... Args>
  detail::binding_status binding_status(Args &&...args) {
    return implementation().template binding_status<TypeArgs...>(
        std::forward<Args>(args)...);
  }

  template <typename... TypeArgs, typename... Args>
  decltype(auto) resolve(Args &&...args) {
    return implementation().template resolve<TypeArgs...>(
        std::forward<Args>(args)...);
  }

  template <typename... TypeArgs, typename... Args>
  decltype(auto) construct(Args &&...args) {
    return implementation().template construct<TypeArgs...>(
        std::forward<Args>(args)...);
  }

  template <typename... TypeArgs, typename... Args>
  decltype(auto) construct_collection(Args &&...args) {
    return implementation().template construct_collection<TypeArgs...>(
        std::forward<Args>(args)...);
  }

  template <typename... TypeArgs, typename... Args>
  decltype(auto) invoke(Args &&...args) {
    return implementation().template invoke<TypeArgs...>(
        std::forward<Args>(args)...);
  }

private:
  implementation_type &implementation() {
    assert(impl_ != nullptr);
    return *impl_;
  }

  template <typename... Args>
  static implementation_type *create(const allocator_type &alloc,
                                     Args &&...args) {
    implementation_allocator allocator(alloc);
    auto *impl = implementation_allocator_traits::allocate(allocator, 1);
    try {
      implementation_allocator_traits::construct(allocator, impl,
                                                 std::forward<Args>(args)...);
    } catch (...) {
      implementation_allocator_traits::deallocate(allocator, impl, 1);
      throw;
    }
    return impl;
  }

  void destroy() noexcept {
    if (impl_ != nullptr) {
      implementation_allocator allocator(impl_->get_allocator());
      implementation_allocator_traits::destroy(allocator, impl_);
      implementation_allocator_traits::deallocate(allocator, impl_, 1);
      impl_ = nullptr;
    }
  }

  implementation_type *impl_;
};

} // namespace dingo

namespace dingo::detail {

template <typename... Ts>
inline constexpr bool container_dependent_false_v = false;

template <typename... Params> struct container_base;
template <typename Param, typename Enable = void>
struct container_base_from_parameter;
template <typename Param, typename Parent, typename Enable = void>
struct container_base_from_static_parent;
struct container_base_runtime_two_parameter_tag {};
struct container_base_static_parent_tag {};
struct container_base_invalid_two_parameter_tag {};

template <typename First>
using container_base_two_parameter_tag_t = std::conditional_t<
    is_container_traits_v<First>, container_base_runtime_two_parameter_tag,
    std::conditional_t<is_static_bindings_v<First> ||
                           is_bindings_wrapper_v<First>,
                       container_base_static_parent_tag,
                       container_base_invalid_two_parameter_tag>>;

template <> struct container_base<> {
  using type = runtime_container<dynamic_container_traits>;
};

template <typename Param> struct container_base<Param> {
  using type = typename container_base_from_parameter<Param>::type;
};

template <typename First, typename Second>
struct container_base<First, Second> {
  using type =
      typename container_base<container_base_two_parameter_tag_t<First>, First,
                              Second>::type;
};

template <typename First, typename Second>
struct container_base<container_base_runtime_two_parameter_tag, First, Second> {
  using type = runtime_container<First, Second, void>;
};

template <typename First, typename Second>
struct container_base<container_base_static_parent_tag, First, Second> {
  using type = typename container_base_from_static_parent<First, Second>::type;
};

template <typename First, typename Second>
struct container_base<container_base_invalid_two_parameter_tag, First, Second> {
  static_assert(container_dependent_false_v<First, Second>,
                "container<T, U> requires runtime traits + allocator or "
                "compile-time bindings + parent container");
};

template <typename First, typename Second, typename Third>
struct container_base<First, Second, Third> {
  static_assert(
      is_container_traits_v<First>,
      "container<T, U, V> requires runtime traits + allocator + parent");
  using type = runtime_container<First, Second, Third>;
};

template <typename First, typename Second, typename Third, typename... Rest>
struct container_base<First, Second, Third, Rest...> {
  static_assert(container_dependent_false_v<First, Second, Third, Rest...>,
                "container<...> expects runtime traits or a single static "
                "bindings source");
};

template <typename... Params>
using container_base_t = typename container_base<Params...>::type;

template <typename Param, typename Enable>
struct container_base_from_parameter {
  static_assert(container_dependent_false_v<Param>,
                "container<T> requires runtime container traits or "
                "compile-time bindings<...>");
};

template <typename Param>
struct container_base_from_parameter<
    Param, std::enable_if_t<is_container_traits_v<Param>>> {
  using type = runtime_container<Param, typename Param::allocator_type, void>;
};

template <typename Param>
struct container_base_from_parameter<
    Param, std::enable_if_t<is_static_bindings_v<Param>>> {
  using type = container_with_static_bindings<Param>;
};

template <typename Param>
struct container_base_from_parameter<
    Param, std::enable_if_t<is_bindings_wrapper_v<Param>>> {
  using static_bindings_type = bindings_wrapper_registry_t<Param>;

  static_assert(is_static_bindings_v<static_bindings_type>,
                "container<bindings<...>> requires a valid compile-time "
                "bindings source");

  using type = container_with_static_bindings<static_bindings_type>;
};

template <typename Param, typename Parent, typename Enable>
struct container_base_from_static_parent {
  static_assert(container_dependent_false_v<Param, Parent>,
                "container<bindings<...>, Parent> requires a valid "
                "compile-time bindings source");
};

template <typename Param, typename Parent>
struct container_base_from_static_parent<
    Param, Parent, std::enable_if_t<is_static_bindings_v<Param>>> {
  using type = container_with_static_bindings<Param, Parent>;
};

template <typename Param, typename Parent>
struct container_base_from_static_parent<
    Param, Parent, std::enable_if_t<is_bindings_wrapper_v<Param>>> {
  using static_bindings_type = bindings_wrapper_registry_t<Param>;

  static_assert(is_static_bindings_v<static_bindings_type>,
                "container<bindings<...>, Parent> requires a valid "
                "compile-time bindings source");

  using type = container_with_static_bindings<static_bindings_type, Parent>;
};

} // namespace dingo::detail
