//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/binding/model.h>
#include <dingo/registration/type_registration.h>
#include <dingo/static/bindings.h>

#include <type_traits>

namespace dingo {
namespace detail {

struct static_container_no_dependency_diagnostics {};

template <typename Container, typename = void>
struct static_container_context_bindings {
  using type = static_bindings<>;
};

template <typename Container>
struct static_container_context_bindings<
    Container, std::void_t<typename Container::context_static_bindings_type>> {
  using type = typename Container::context_static_bindings_type;
};

template <typename Container>
using static_container_context_bindings_t =
    typename static_container_context_bindings<Container>::type;

template <typename StaticRegistry, bool HasParent>
struct static_container_dependency_diagnostics_base;

template <typename... Registrations>
struct static_container_dependency_diagnostics_base<
    static_bindings<Registrations...>, false>
    : static_registry_dependency_diagnostics<
          typename static_bindings<Registrations...>::interface_bindings,
          binding_model<Registrations>...> {};

template <typename... Registrations>
struct static_container_dependency_diagnostics_base<
    static_bindings<Registrations...>, true>
    : static_container_no_dependency_diagnostics {};

} // namespace detail
} // namespace dingo
