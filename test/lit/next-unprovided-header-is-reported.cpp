// A public header the next implementation does not provide yet is reported
// instead of falling back to the current implementation.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: dingo/factory/callable.h is not provided by the next
// implementation

#include <dingo/factory/callable.h>
