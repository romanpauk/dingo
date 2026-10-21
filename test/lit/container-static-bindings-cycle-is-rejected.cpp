// The static bindings of a mixed container are known when the container type
// is formed, so a cycle among them is rejected then, whatever the container
// registers at runtime.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: recursive dependency detected:

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct a {};
struct b {};
struct runtime_only {};

using source =
    dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, dingo::storage<a>,
                                dingo::dependencies<b &, runtime_only &>>,
                    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<b>,
                                dingo::dependencies<a &>>>;

int main() {
  dingo::container<source> instance;
  (void)instance;
}
