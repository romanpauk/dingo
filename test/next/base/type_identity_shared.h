//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// Deliberately declares nothing that names std::unique_ptr<service>. Each
// translation unit must be the first to spell the type its own way, because
// GCC records whichever template-id first created the specialization, and
// names the type in its signatures by it.

#include <dingo/container.h>
#include <dingo/core/primitives/shape.h>

namespace next_type_identity {

struct service {
  service() : value(7) {}

  int value;
};

// The identity of std::unique_ptr<service>, constant-evaluated in the unit
// that spells it, and the identity of service, which the type is a layer of.
dingo::core::type_id bare_spelling_id();
dingo::core::type_id defaulted_spelling_id();
dingo::core::type_id defaulted_spelling_leaf_id();

void register_defaulted_spelling(dingo::container<> &container);
int resolve_defaulted_spelling(dingo::container<> &container);

} // namespace next_type_identity
