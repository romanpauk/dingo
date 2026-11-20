// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct made {};

struct element {
  explicit element(made) {}
};

// The elements of a bounded array a default factory makes are
// value-initialized, which these cannot be.
int main() {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<element[2]>>();
}

// CHECK: bounded array elements cannot be initialized
