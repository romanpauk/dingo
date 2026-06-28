// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>

int main() {
  dingo::container<> container;
  auto values = container.construct_collection<int>();
  (void)values;
}

// CHECK: missing collection_traits specialization for type T
