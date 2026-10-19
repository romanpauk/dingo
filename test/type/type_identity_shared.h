//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Deliberately declares nothing that names std::unique_ptr<service>. Each
// translation unit must be the first to spell the type its own way, because
// GCC records whichever template-id first created the specialization.

#include <dingo/base/type_descriptor.h>
#include <dingo/container.h>

namespace dingo {
namespace type_identity_test {

struct service {
  service() : value(7) {}

  int value;
};

// Constant-evaluated descriptor of std::unique_ptr<service>, spelled by the
// defining translation unit.
type_descriptor bare_spelling_descriptor();
type_descriptor defaulted_spelling_descriptor();

void register_defaulted_spelling(container<> &container);
int resolve_defaulted_spelling(container<> &container);

} // namespace type_identity_test
} // namespace dingo
