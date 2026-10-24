//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

// Names std::unique_ptr<service, std::default_delete<service>> first.

#include "base/type_identity_shared.h"

#include <dingo/storage/unique.h>

#include <memory>

namespace next_type_identity {

using defaulted_spelling =
    std::unique_ptr<service, std::default_delete<service>>;

dingo::type_id defaulted_spelling_id() {
  constexpr dingo::type_id id = dingo::type_id_v<defaulted_spelling>;
  return id;
}

dingo::type_id defaulted_spelling_leaf_id() {
  constexpr dingo::type_id id =
      dingo::type_tree_v<defaulted_spelling>.nodes[1].type;
  return id;
}

void register_defaulted_spelling(dingo::container<> &container) {
  container.register_type<dingo::scope<dingo::unique>,
                          dingo::storage<defaulted_spelling>>();
}

int resolve_defaulted_spelling(dingo::container<> &container) {
  return container.resolve<defaulted_spelling>()->value;
}

} // namespace next_type_identity
