//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/none.h>
#include <dingo/lookup/lookup.h>

#include <type_traits>

namespace dingo::detail {

template <typename LookupEntry, bool = std::is_void_v<LookupEntry>>
struct lookup_entry_cardinality {
  using type = typename LookupEntry::cardinality;
};

template <typename LookupEntry>
struct lookup_entry_cardinality<LookupEntry, true> {
  using type = ::dingo::one;
};

} // namespace dingo::detail
