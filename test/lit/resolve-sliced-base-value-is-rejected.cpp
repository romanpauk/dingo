// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

struct base {
  int marker = 0;
};

struct derived : base {};

// A value of a base would be a copy of part of the registered object.
int main() {
  dingo::static_container<dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<derived>,
                  dingo::interfaces<base>>>>
      container;
  (void)container.resolve<base>();
}

// CHECK: a value of a base of the registered type would slice it
