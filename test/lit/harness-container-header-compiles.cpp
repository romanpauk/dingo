// The container header is self-contained: a program that includes only it
// declares, and resolves from, a container.
// REQUIRES: linux
// RUN: %dingo_cxx -c %s -o %t.o

#include <dingo/container.h>

int main() {
  dingo::container<> container;
  return container.template resolve<int>();
}
