// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>

// A complete scope tag without a dingo::scope_traits specialization.
struct custom_storage_tag {};
struct value {};

int main() {
  dingo::container<> container;
  container
      .register_type<dingo::scope<custom_storage_tag>, dingo::storage<value>>();
}

// CHECK: registered storage tag must be complete and specialize
// CHECK-SAME: dingo::scope_traits
