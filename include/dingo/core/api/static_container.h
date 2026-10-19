//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// dingo/static_container.h of the next implementation.

#include <dingo/core/api/registration.h>
#include <dingo/core/backend/static_backend.h>
#include <dingo/core/primitives/exceptions.h> // IWYU pragma: export

namespace dingo {

template <typename Bindings> class static_container;

// A container of the bindings of its type.
template <typename... Binds> class static_container<bindings<Binds...>> {
public:
  template <typename Request> Request resolve() {
    return backend_.template resolve<Request>();
  }

private:
  core::static_backend<
      typename core::bind_registration<Binds>::type::binding...>
      backend_;
};

} // namespace dingo
