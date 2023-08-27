#pragma once

namespace dingo {
template <typename> class resolving_context;

template <typename Container> class constructible_i {
  public:
    virtual ~constructible_i() = default;

    virtual void construct(resolving_context<Container>& context, int phase) = 0;
};

} // namespace dingo
