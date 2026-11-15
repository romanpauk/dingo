//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// What the generated programs of a static container need of it: a static
// container binds what a probe registers in its type, and constructs the
// storage and the factory of a binding itself.

#include "model/probe.h"

#include <dingo/static/binding.h>
#include <dingo/static_container.h>

#include <type_traits>

namespace dingo::model {

template <typename Model>
struct statically_constructible
    : std::bool_constant<
          detail::binding_storage_is_default_constructible<Model>::value &&
          detail::binding_factory_is_default_constructible<Model>::value> {};

template <typename... Bindings, typename Traits, typename Binding>
struct container_with<static_container<bindings<Bindings...>, Traits>,
                      Binding> {
  using type = static_container<bindings<Bindings..., Binding>, Traits>;
  static constexpr bool binds = true;
};

} // namespace dingo::model
