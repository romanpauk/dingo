// The dependencies static bindings declare are known when the container is
// compiled, so a cycle through a binding that is not shared_cyclical is
// rejected then, with its path.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/shared_cyclical.h>

struct first;
struct second;

struct first {
  explicit first(second &);
};

struct second {
  explicit second(first &);
};

first::first(second &) {}
second::second(first &) {}

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared_cyclical>, dingo::storage<first>,
                  dingo::dependencies<second &>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<second>,
                  dingo::dependencies<first &>>>;
  dingo::static_container<bindings> container;
  (void)container;
}

// CHECK: recursive dependency detected:
