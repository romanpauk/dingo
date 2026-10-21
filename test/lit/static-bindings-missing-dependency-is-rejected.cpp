// A static container requests the dependencies its bindings declare when it is
// compiled, so a container whose binding declares a dependency no binding
// serves is rejected when it is formed, whether or not it is requested.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: type not found:

#include <dingo/static_container.h>
#include <dingo/storage/unique.h>

struct dependency {};
struct service {
  explicit service(dependency &) {}
};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<service>,
                dingo::dependencies<dependency &>>>;

int main() {
  dingo::static_container<source> instance;
  (void)instance;
}
