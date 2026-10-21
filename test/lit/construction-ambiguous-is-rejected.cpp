// A type whose constructors take one argument of different types has no
// constructor the container can detect, so constructing it is rejected when
// it is compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: no constructor of {{.*}} is detected

#include <dingo/container.h>

struct ambiguous_type {
  ambiguous_type(int) {}
  ambiguous_type(float) {}
};

int main() {
  dingo::container<> container;
  auto value = container.construct<ambiguous_type>();
  (void)value;
}
