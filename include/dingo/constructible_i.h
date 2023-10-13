#pragma once

#include <dingo/config.h>

namespace dingo {

class constructible_i {
  public:
    // Note: derived class has to be trivially destructible
    // virtual ~constructible_i() = default;
    virtual void construct() = 0;
};

} // namespace dingo
