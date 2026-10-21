// An array of unknown bound has no size to construct it with, so a binding
// that stores one without a factory is rejected when it is compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: has no default factory; the binding needs a factory<>

#include <dingo/container.h>
#include <dingo/storage/unique.h>

#include <memory>

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>,
                          dingo::storage<std::unique_ptr<int[]>>>();
  (void)container.resolve<std::unique_ptr<int[]>>();
}
