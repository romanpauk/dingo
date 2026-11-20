// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>

struct ambiguous_type {
  ambiguous_type(int) {}
  ambiguous_type(float) {}
};

// The static container rejects it as it compiles; the runtime container,
// which learns only at run time whether the type is bound, throws
// type_not_found.
int main() {
  dingo::static_container<dingo::bindings<>> container;
  auto value = container.construct<ambiguous_type>();
  (void)value;
}

// CHECK: class T construction not detected or ambiguous
