// An incomplete interface is no interface the stored type can be converted
// to, so a binding that declares one is rejected when it is registered.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: no interface the binding of {{.*}} declares can be served

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct service {};
struct incomplete_interface;

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>, dingo::storage<service>,
                          dingo::interfaces<incomplete_interface>>();
}
