// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct animal {
  virtual ~animal() = default;
};

struct dog : animal {};

struct traits : dingo::dynamic_container_traits {
  using view_definition_type =
      dingo::views<dingo::typed<std::size_t, animal, dingo::one>,
                   dingo::typed<std::size_t, animal, dingo::many>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<dog>,
                          dingo::interfaces<animal>>();
}

// CHECK: conflicting dingo view definitions for interface/key domain
