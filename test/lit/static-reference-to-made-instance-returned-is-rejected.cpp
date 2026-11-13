// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/unique.h>

struct A {
  A() {}
  int value = 7;
};

struct reader {
  explicit reader(A &instance) : value(instance.value) {}
  int value;
};

// A static container knows the reader is returned to the caller, which a
// reference to the unique instance would outlive.
int main() {
  dingo::static_container<dingo::bindings<
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<A>>,
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<reader>>>>
      container;
  (void)container.resolve<reader>();
}

// CHECK: would dangle in what is returned to the caller
