//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

#pragma once

#include <dingo/config.h>

namespace dingo {
class resettable {
  public:
    virtual ~resettable() = default;
    virtual void reset() = 0;
};
} // namespace dingo
