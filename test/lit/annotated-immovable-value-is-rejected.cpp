// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct A {
  A() = default;
  A(const A &) = delete;
  A(A &&) = delete;
};
struct tag {};
struct B {
  explicit B(dingo::annotated<const A, tag>) {}
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>, dingo::storage<A>,
                          dingo::interfaces<dingo::annotated<A, tag>>>();
  container.register_type<
      dingo::scope<dingo::unique>, dingo::storage<B>,
      dingo::factory<dingo::constructor<B(dingo::annotated<const A, tag>)>>>();
  container.resolve<B>();
}

// CHECK: an annotated or keyed value that can be neither moved nor copied
// CHECK-SAME: cannot be passed by value
