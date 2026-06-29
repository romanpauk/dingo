// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct animal {
  virtual ~animal() = default;
};

struct dog : animal {};

struct traits : dingo::dynamic_container_traits {
  using index_definition_type =
      dingo::selectors<dingo::selector<animal, dingo::no_key, dingo::one>,
                       dingo::selector<animal, dingo::no_key, dingo::many>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<dog>,
                          dingo::interfaces<animal>>();
}

// CHECK: conflicting dingo selector definitions for interface/key domain
