#pragma once

namespace dingo {
class resettable_i {
  public:
    virtual ~resettable_i() = default;
    virtual void reset() = 0;
};
} // namespace dingo