// The local static bindings of a runtime registration are known when the
// registration is compiled, so a cycle among them is rejected then.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: recursive dependency detected:

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct b;

struct a {
  explicit a(b &) {}
};

struct b {
  explicit b(a &) {}
};

int main() {
  dingo::container<> container;
  container.register_type<
      dingo::scope<dingo::shared>, dingo::storage<int>,
      dingo::bindings<
          dingo::bind<dingo::scope<dingo::shared>, dingo::storage<a>,
                      dingo::dependencies<b &>>,
          dingo::bind<dingo::scope<dingo::shared>, dingo::storage<b>,
                      dingo::dependencies<a &>>>>();
}
