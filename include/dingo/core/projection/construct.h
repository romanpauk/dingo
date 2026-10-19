//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

// The default factory of a stored shape: the leaf is value-initialized and
// every layer is made from its element by its layer traits.

#include <dingo/core/base/mechanics.h>
#include <dingo/core/projection/layer_traits.h>
#include <dingo/core/vocabulary/vocabulary.gen.h>

#include <new>
#include <type_traits>

namespace dingo::core {

template <typename Stored> Stored make_default() {
  using traits = layer_traits<Stored>;
  if constexpr (traits::kind == layer_kind::leaf) {
    return Stored();
  } else {
    using element = type_list_at_t<0, typename traits::arguments>;
    return traits::make(make_default<element>());
  }
}

// Constructs the stored shape at `where`; arrays are value-initialized in
// place.
template <typename Stored> void construct_default(void *where) {
  if constexpr (std::is_array_v<Stored>) {
    ::new (where) Stored();
  } else {
    ::new (where) Stored(make_default<Stored>());
  }
}

} // namespace dingo::core
