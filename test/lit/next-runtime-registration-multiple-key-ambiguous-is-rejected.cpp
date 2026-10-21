// One key value of a registration is the key of every interface the binding
// declares, so it is accepted for a binding of two keyed interfaces.
// REQUIRES: linux
// RUN: %dingo_next_cxx -c %s -o %t.o

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct processor {
  virtual ~processor() = default;
};

struct animal {
  virtual ~animal() = default;
};

struct processor_animal : processor, animal {};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, processor>,
                     dingo::associative<int, animal>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<processor_animal>,
                          dingo::interfaces<processor, animal>>(
      dingo::key_value{7});
}
