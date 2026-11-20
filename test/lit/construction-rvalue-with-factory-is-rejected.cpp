// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/factory/constructor.h>

struct A {};

// A factory makes a value, which an rvalue reference request does not take.
int main() {
  dingo::container<> container;
  (void)container.construct<A &&>(dingo::constructor<A()>());
}

// CHECK: construct<T &&>(factory) cannot take what the factory makes
