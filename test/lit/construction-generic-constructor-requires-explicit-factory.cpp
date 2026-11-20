// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>

struct generic_constructor_type {
  template <class T, class U> generic_constructor_type(T, U) {}
};

// The static container rejects it as it compiles; the runtime container,
// which learns only at run time whether the type is bound, throws
// type_not_found.
int main() {
  dingo::static_container<dingo::bindings<>> container;
  auto value = container.construct<generic_constructor_type>();
  (void)value;
}

// CHECK: generic constructor detected; use explicit factory<constructor<...>>
