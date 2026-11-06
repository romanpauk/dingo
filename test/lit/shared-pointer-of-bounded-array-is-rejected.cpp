// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <memory>

struct A {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<A[2]>>();
  (void)container.construct<std::shared_ptr<A[2]>>();
}

// CHECK: std::shared_ptr of a bounded array is not supported
