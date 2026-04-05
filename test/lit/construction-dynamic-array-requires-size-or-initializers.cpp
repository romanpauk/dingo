// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/type/type_traits.h>

int main() {
    [[maybe_unused]] auto values = dingo::detail::make_dynamic_array<int>();
}

// CHECK: dynamic arrays require an explicit size via a custom factory or element initializers
