//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/container.h>

namespace dingo {
namespace incomplete_resolve_test {

struct service;
struct handle_service;

struct consumer {
  explicit consumer(service &value);

  service *value;
};

void register_service(container<> &container);

// Registers a shared scope that stores a std::shared_ptr<handle_service>.
void register_handle_service(container<> &container);

// The value a handle_service holds, read where its type is complete.
int value_of(handle_service &service);

} // namespace incomplete_resolve_test
} // namespace dingo
