#pragma once

#include <dingo/config.h>

namespace dingo {

class constructible_i {
  public:
    virtual ~constructible_i() = default;
    virtual void construct() = 0;
};

} // namespace dingo
