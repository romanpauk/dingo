// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s
// MSVC does not instantiate this empty-index diagnostic path reliably.
// UNSUPPORTED: msvc

#include <dingo/container.h>

#include <cstddef>

struct processor {
  int value = 0;
};

int main() {
  dingo::container<> container;
  return container.resolve<processor>(std::size_t(1)).value;
}

// CHECK: indexed registration or lookup has no matching dingo selector
// CHECK: definition for interface/key
