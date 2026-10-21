// A pointer to an incomplete type cannot be converted to a pointer of an
// interface, so a binding that stores one with a declared interface is
// rejected when it is registered.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: does not derive from the interface

#include <dingo/container.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/unique.h>

struct service_interface {
  virtual ~service_interface() = default;
};

struct incomplete_service;

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::unique>,
                          dingo::storage<incomplete_service *>,
                          dingo::interfaces<service_interface>>(
      dingo::callable([]() -> incomplete_service * { return nullptr; }));
}
