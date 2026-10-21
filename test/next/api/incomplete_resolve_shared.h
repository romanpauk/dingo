//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>

namespace next_incomplete_resolve {

// Complete only in incomplete_resolve_fixture.cpp, which registers it.
struct service;

struct consumer {
  explicit consumer(service &value);

  service *value;
};

void register_service(dingo::container<> &container);

} // namespace next_incomplete_resolve
