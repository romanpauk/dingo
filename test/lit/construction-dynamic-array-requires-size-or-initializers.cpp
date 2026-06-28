// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/type/type_traits.h>

int main() {
  auto values = dingo::detail::make_dynamic_array<int>();
  (void)values;
}

// CHECK: dynamic arrays require an explicit size via a custom factory or
// element initializers
