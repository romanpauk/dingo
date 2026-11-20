// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct immovable {
  immovable() = default;
  immovable(const immovable &) = delete;
};

// resolve<T &&>() moves the value out of the object it refers to.
int main() {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::unique>, dingo::storage<immovable>>();
  (void)container.resolve<immovable &&>();
}

// CHECK: resolve<T &&>() moves the value out of the object it refers to
