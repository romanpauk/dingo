// A published leaf is converted to an interface before it is constructed, which
// a virtual base does not allow, so the binding is rejected.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: shared_cyclical cannot publish

#include <dingo/container.h>
#include <dingo/storage/shared_cyclical.h>

struct iface {
  virtual ~iface() = default;
};

struct impl : virtual iface {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared_cyclical>,
                          dingo::storage<impl>, dingo::interfaces<iface>>();
}
