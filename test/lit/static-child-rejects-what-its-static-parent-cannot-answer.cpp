// A static child of a static parent knows every binding its request can name
// when the request is compiled, so a request neither of them answers is
// rejected then.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

struct service {
  virtual ~service() = default;
};

struct config {
  config() {}
};

int main() {
  using parent_type = dingo::static_container<dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<config>>>>;
  parent_type parent;
  dingo::static_container<dingo::bindings<>, parent_type> child(&parent);
  (void)child.resolve<service &>();
}

// CHECK: type not found:
