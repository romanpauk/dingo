// A dingo::type_conversion_traits specialization declares the access its
// conversion needs, so a conversion through one that declares no
// required_access is rejected when it is compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: declares no required_access

#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/type/type_conversion_traits.h>

struct target {
  explicit target(int initial) : value(initial) {}
  int value;
};

struct source : target {
  source() : target(5) {}
};

namespace dingo {
template <> struct type_conversion_traits<target, source> {
  static target convert(const source &from) { return target(from.value * 10); }
};
} // namespace dingo

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<source>,
                          dingo::interfaces<target>>();
  return container.resolve<target>().value;
}
