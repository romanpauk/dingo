// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct service {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>,
                          dingo::key_type<std::size_t, 0>>();
}

// CHECK: dingo::key_type<T, V> registration keys require a static fixed
// CHECK: runtime-key request
