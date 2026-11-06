// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/unique.h>

#include <optional>

struct A {
  A() {}
};

// No dependency can initialize the member, so detection would fall back to
// no arguments and leave it empty; the aggregate is rejected instead.
struct consumer {
  std::optional<A> held;
};

// The static container rejects it as it compiles; the runtime container,
// which learns only at run time whether the type is bound, throws
// type_not_found.
int main() {
  dingo::static_container<dingo::bindings<
      dingo::bind<dingo::scope<dingo::unique>, dingo::storage<A>>>>
      container;
  (void)container.construct<consumer>();
}

// CHECK: class T construction not detected or ambiguous
