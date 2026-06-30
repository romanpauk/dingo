// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct animal {
  virtual ~animal() = default;
};

struct dog : animal {};

struct traits : dingo::dynamic_container_traits {
  using query_definition_type =
      dingo::queries<dingo::typed<animal, std::size_t, dingo::one>,
                     dingo::typed<animal, std::size_t, dingo::many>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<dog>,
                          dingo::interfaces<animal>>();
}

// CHECK: conflicting dingo query definitions for interface/key domain
