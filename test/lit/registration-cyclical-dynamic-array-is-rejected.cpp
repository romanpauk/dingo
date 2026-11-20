// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared_cyclical.h>

#include <memory>

struct A {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared_cyclical>,
                          dingo::storage<std::shared_ptr<A[]>>>();
}

// CHECK: the scope does not store this type
