// A fixed key may be a value of a structural class type, which selects the
// binding of an equal key in a runtime and in a static container.
// REQUIRES: linux, can-run-built-executables
// RUN: %dingo_next_cxx %s -o %t && %t

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

struct structural_key {
  int value;
  constexpr bool operator==(const structural_key &) const = default;
};

using key_type = dingo::key_type<structural_key, structural_key{1}>;

struct service {
  service() : id(3) {}
  int id;
};

struct consumer {
  explicit consumer(dingo::dependency<service &, key_type> selected)
      : chosen(selected) {}
  service &chosen;
};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>,
                          key_type>();
  using bindings =
      dingo::bindings<dingo::bind<dingo::scope<dingo::shared>,
                                  dingo::storage<service>, key_type>>;
  dingo::static_container<bindings> fixed;
  bool found = container.resolve<service &>(structural_key{1}).id == 3 &&
               container.construct<consumer>().chosen.id == 3 &&
               fixed.resolve<service &>(structural_key{1}).id == 3 &&
               fixed.construct<consumer>().chosen.id == 3;
  return found ? 0 : 1;
}
