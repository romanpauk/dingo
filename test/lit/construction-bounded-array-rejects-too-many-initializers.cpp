// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/factory/constructor_traits.h>

int main() {
    [[maybe_unused]] auto values =
        dingo::constructor_traits<int[2]>::construct(1, 2, 3);
}

// CHECK: too many initializers for bounded array construction
