// A type that does not derive from an interface cannot be served as it, so a
// binding that declares an unrelated interface is rejected when it is
// registered.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: no interface the binding of {{.*}} declares can be served

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct service {};
struct unrelated_interface {
  virtual ~unrelated_interface() = default;
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>, dingo::storage<service>,
                          dingo::interfaces<unrelated_interface>>();
}
