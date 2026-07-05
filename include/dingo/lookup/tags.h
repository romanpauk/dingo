//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/core/none.h>

namespace dingo {
template <typename... Definitions> struct lookups {};
struct one {};
struct many {};

namespace detail {
template <typename Interface, typename KeyDomain, typename Cardinality,
          typename Backend = none_t>
struct lookup_definition {};
} // namespace detail

} // namespace dingo
