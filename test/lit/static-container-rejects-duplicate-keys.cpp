// A static container holds one binding of a key of an interface unless its
// lookup is declared many, so a second binding of one key is rejected when the
// container type is formed.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: lookup already registered: interface

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

struct iface {
  virtual ~iface() = default;
};
struct first : iface {};
struct second : iface {};

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<first>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<second>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>>;
  dingo::static_container<bindings> container;
  (void)container;
}
