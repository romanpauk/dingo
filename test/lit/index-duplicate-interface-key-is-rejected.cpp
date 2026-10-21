// The lookups of container traits declare the domain of an interface and a
// key once, so an associative domain declared twice is rejected when the
// container is compiled, even when both declarations agree.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: the lookup of
// CHECK-SAME: is declared twice

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

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
                     dingo::interfaces<processor>>(
          dingo::key_value{std::size_t(1)});
}
