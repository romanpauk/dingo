// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/registration/type_registration.h>

template <typename T> struct fake_storage {
    using type = T;
    template <typename U> using rebind_t = dingo::storage<U>;
};

template <typename T> struct fake_factory {
    using type = T;
    template <typename U> using rebind_t = dingo::factory<U>;
};

using registration = dingo::type_registration<
    dingo::scope<int>,
    fake_storage<void>,
    fake_factory<void>>;
using interface_type = typename registration::interface_type;

int main() {
    return sizeof(interface_type);
}

// CHECK: failed to deduce an interface type
