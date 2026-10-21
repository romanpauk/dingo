// A scope is a tag type whose dingo::scope_traits hold its descriptor, so a
// registration with a scope tag that has none is rejected when it is
// compiled.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: is not a scope: it has no dingo::scope_traits

#include <dingo/container.h>

struct undeclared_scope;

int main() {
  dingo::container<> container;
  container
      .register_type<dingo::scope<undeclared_scope>, dingo::storage<int>>();
}
