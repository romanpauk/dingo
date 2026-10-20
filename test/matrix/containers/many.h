//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The containers of a case whose static bindings share a domain, an interface
// and a key. The next implementation rejects a second binding of a domain its
// lookups declare one, as the behavior change
// duplicates_rejected_in_both_domains of the spec declares, so the containers
// declare every domain many there; the current one accepts it without traits.

#include "matrix/containers/storage.h"

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>

namespace dingo::matrix {

#if defined(DINGO_IMPLEMENTATION_NEXT)
struct many_container_traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::base<dingo::many>>;
};

struct many_static_container_traits : dingo::static_container_traits {
  using lookup_definition_type = dingo::lookups<dingo::base<dingo::many>>;
};

template <typename Bindings>
using many_container = dingo::container<Bindings, many_container_traits>;

template <typename Bindings>
using many_static_container =
    dingo::static_container<Bindings, many_static_container_traits>;
#else
template <typename Bindings> using many_container = dingo::container<Bindings>;

template <typename Bindings>
using many_static_container = dingo::static_container<Bindings>;
#endif

} // namespace dingo::matrix
