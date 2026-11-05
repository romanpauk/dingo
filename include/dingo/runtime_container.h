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
#include <dingo/runtime/container.h>
#include <dingo/runtime/container_traits.h>
#include <dingo/runtime/registry.h>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

namespace dingo {

// The runtime container. Registered bindings, registration containers and
// child containers point back at the resolve front-end, so it lives at a
// stable heap address and runtime_container moves by transferring ownership.
template <typename ContainerTraits = runtime_container_traits,
          typename Allocator = typename ContainerTraits::allocator_type,
          typename ParentContainer = void>
class runtime_container {
  static_assert(detail::is_container_traits_v<ContainerTraits>,
                "runtime_container requires runtime container traits");

public:
  using implementation_type =
      detail::runtime_container_impl<ContainerTraits, Allocator,
                                     ParentContainer>;

private:
  using implementation_allocator = typename std::allocator_traits<
      Allocator>::template rebind_alloc<implementation_type>;
  using implementation_allocator_traits =
      std::allocator_traits<implementation_allocator>;

  template <typename, typename> friend struct detail::container_implementation;

public:
  using container_traits_type = ContainerTraits;
  using allocator_type = Allocator;
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

// The runtime container under its short name.
template <typename... Params> using container = runtime_container<Params...>;

} // namespace dingo
