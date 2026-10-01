//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Names std::unique_ptr<service, std::default_delete<service>> first.

#include "type_identity_shared.h"

#include <dingo/storage/unique.h>

#include <memory>

namespace dingo {
namespace type_identity_test {

using defaulted_spelling =
    std::unique_ptr<service, std::default_delete<service>>;

type_descriptor defaulted_spelling_descriptor() {
  constexpr auto descriptor = describe_type<defaulted_spelling>();
  return descriptor;
}

void register_defaulted_spelling(container<> &container) {
  container
      .template register_type<scope<unique>, storage<defaulted_spelling>>();
}

int resolve_defaulted_spelling(container<> &container) {
  return container.template resolve<defaulted_spelling>()->value;
}

} // namespace type_identity_test
} // namespace dingo
