// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<processor, std::size_t>,
      dingo::lookup<processor, dingo::runtime_key<std::size_t>, dingo::one>>;
};

int main() {
  dingo::container<traits> container;
  container.register_indexed_type<dingo::scope<dingo::shared>,
                                  dingo::storage<processor>,
                                  dingo::interfaces<processor>>(std::size_t(1));
}

// CHECK: duplicate dingo lookup definition for interface/key
