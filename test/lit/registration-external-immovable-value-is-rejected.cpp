// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/external.h>

struct immovable {
  immovable() = default;
  immovable(const immovable &) = delete;
};

// An external scope keeps a copy of a value it is given; this one has to be
// given by reference.
int main() {
  dingo::container<> container;
  container
      .register_type<dingo::scope<dingo::external>, dingo::storage<immovable>>(
          immovable{});
}

// CHECK: an external scope keeps a copy of the value it is given
