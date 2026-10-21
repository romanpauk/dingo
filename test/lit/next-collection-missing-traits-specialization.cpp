// A collection is filled through its dingo::collection_traits, so a
// collection request of a type without a specialization is rejected when it
// is compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: is not a collection: it has no dingo::collection_traits

#include <dingo/container.h>

int main() {
  dingo::container<> container;
  auto values = container.construct_collection<int>();
  (void)values;
}
