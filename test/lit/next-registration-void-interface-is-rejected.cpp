// void is no interface a request can be served by, so a binding that declares
// it is rejected when it is registered.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: no interface the binding of {{.*}} declares can be served

#include <dingo/container.h>
#include <dingo/storage/unique.h>

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>, dingo::storage<int>,
                          dingo::interfaces<void>>();
}
