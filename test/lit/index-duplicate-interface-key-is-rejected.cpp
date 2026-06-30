// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {};

struct traits : dingo::dynamic_container_traits {
  using view_definition_type =
      dingo::views<dingo::associative<std::size_t, processor>,
                   dingo::associative<std::size_t, processor, dingo::one>>;
};

int main() {
  dingo::container<traits> container;
  container.register_indexed_type<dingo::scope<dingo::shared>,
                                  dingo::storage<processor>,
                                  dingo::interfaces<processor>>(std::size_t(1));
}

// CHECK: duplicate dingo view definition for interface/key
