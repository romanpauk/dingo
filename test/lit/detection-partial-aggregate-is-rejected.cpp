// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

#include <optional>

struct A {
  A() {}
};

// No dependency can initialize the member, so detection would fall back to
// no arguments and leave it empty; the aggregate is rejected instead.
struct consumer {
  std::optional<A> held;
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>, dingo::storage<A>>();
  (void)container.construct<consumer>();
}

// CHECK: class T construction not detected or ambiguous
