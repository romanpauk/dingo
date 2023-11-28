//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

namespace dingo {
class resettable_i {
  public:
    virtual ~resettable_i() = default;
    virtual void reset() = 0;
};
} // namespace dingo