// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct A {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<A &>>();
}

// CHECK: the scope does not store this type
