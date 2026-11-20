// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct A {};

int main() {
  dingo::static_container<dingo::bindings<
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<A>>>>
      container;
  container.resolve<A &>();
}

// CHECK: static_container cannot return a reference or a pointer to an
// instance the resolution constructs and does not keep
