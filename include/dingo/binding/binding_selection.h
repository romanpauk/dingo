//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

namespace dingo {
namespace detail {

enum class binding_status {
  found,
  not_found,
  ambiguous,
};

} // namespace detail
} // namespace dingo
