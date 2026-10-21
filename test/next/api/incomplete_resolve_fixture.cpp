//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// The translation unit where the service of incomplete_resolve.cpp is
// complete, which registers it.

#include <dingo/storage/external.h>

#include "incomplete_resolve_shared.h"

namespace next_incomplete_resolve {

struct service {
  int value = 7;
};

consumer::consumer(service &dependency) : value(&dependency) {}

void register_service(dingo::container<> &container) {
  static service instance{};
  container
      .register_type<dingo::scope<dingo::external>, dingo::storage<service &>>(
          instance);
}

} // namespace next_incomplete_resolve
