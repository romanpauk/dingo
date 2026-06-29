// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {};
struct animal {};

struct traits : dingo::dynamic_container_traits {
  using index_definition_type =
      dingo::selectors<dingo::associative<animal, std::size_t>>;
};

int main() {
  dingo::container<traits> container;
  container.register_indexed_type<dingo::scope<dingo::shared>,
                                  dingo::storage<processor>,
                                  dingo::interfaces<processor>>(std::size_t(1));
}

// CHECK: indexed registration or lookup has no matching dingo selector
// definition for interface/key
