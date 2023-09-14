#pragma once

#include <dingo/config.h>
#include <dingo/factory/function.h>

namespace dingo {

template <typename T> struct callable {
    callable(T fn) : fn_(fn) {}

    template <typename Type, typename Context> Type construct(Context& ctx) {
        return detail::function_impl<decltype(&T::operator())>::construct(fn_, ctx);
    }

    template <typename Type, typename Context> void construct(void* ptr, Context& ctx) {
        // TODO
        new (ptr) decay_t<Type>(detail::function_impl<decltype(&T::operator())>::construct(fn_, ctx));
    }

  private:
    T fn_;
};

} // namespace dingo