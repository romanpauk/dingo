#pragma once

#include <dingo/config.h>
#include <dingo/factory/function.h>

namespace dingo {

template <typename T> struct callable {
    callable(T fn) : fn_(fn) {}

    template <typename Type, typename Context, typename Container> Type construct(Context& ctx, Container& container) {
        return detail::function_impl<decltype(&T::operator())>::construct(fn_, ctx, container);
    }

    template <typename Type, typename Context, typename Container>
    void construct(void* ptr, Context& ctx, Container& container) {
        // TODO
        new (ptr) decay_t<Type>(detail::function_impl<decltype(&T::operator())>::construct(fn_, ctx, container));
    }

  private:
    T fn_;
};

} // namespace dingo