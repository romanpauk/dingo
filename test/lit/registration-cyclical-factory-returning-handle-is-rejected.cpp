// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/factory/function.h>
#include <dingo/storage/shared_cyclical.h>

#include <memory>

struct A {};

std::shared_ptr<A> make() { return std::make_shared<A>(); }

// A cyclical scope constructs the object in place, at an address published
// before; a handle the factory returns owns an object of its own.
int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared_cyclical>,
                          dingo::storage<std::shared_ptr<A>>,
                          dingo::factory<dingo::function<&make>>>();
  (void)container.resolve<A &>();
}

// CHECK: returns the object, not a handle of it
