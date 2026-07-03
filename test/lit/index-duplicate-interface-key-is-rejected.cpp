// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>
#include <type_traits>

struct processor {};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, processor>,
                     dingo::associative<std::size_t, processor, dingo::one>>;
};

int main() {
  dingo::container<traits> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<processor>,
                     dingo::interfaces<processor>>(dingo::key{std::size_t(1)});
}

// CHECK: duplicate dingo lookup definition for interface/key
