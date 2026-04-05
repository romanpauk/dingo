// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>

struct generic_constructor_type {
    template <class T, class U> generic_constructor_type(T, U) {}
};

int main() {
    dingo::container<> container;
    [[maybe_unused]] auto value = container.construct<generic_constructor_type>();
}

// CHECK: generic constructor detected; use explicit factory<constructor<...>>
