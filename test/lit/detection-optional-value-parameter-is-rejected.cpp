// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/unique.h>

#include <optional>

struct A {
  A() {}
};

// Shape detection cannot take a std::optional by value: its converting
// constructor and the detection argument's conversion to the optional cannot
// be ranked, on any conforming compiler.
struct consumer {
  explicit consumer(std::optional<A>) {}
};

template <> struct dingo::is_auto_constructible<consumer> : std::true_type {};

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
