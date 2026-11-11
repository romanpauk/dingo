//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>

#include <memory>

#include "incomplete_resolve_shared.h"

namespace dingo {
namespace incomplete_resolve_test {

struct service {
  int value = 7;
};

struct handle_service {
  handle_service() {}
  int value = 9;
};

consumer::consumer(service &dependency) : value(&dependency) {}

void register_service(container<> &container) {
  static service instance{};
  container.template register_type<scope<external>, storage<service &>>(
      instance);
}

void register_handle_service(container<> &container) {
  container.template register_type<scope<shared>,
                                   storage<std::shared_ptr<handle_service>>>();
}

int value_of(handle_service &service) { return service.value; }

} // namespace incomplete_resolve_test
} // namespace dingo
