// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/storage/shared.h>

#include <cstddef>
#include <tuple>

struct processor {};

struct traits : dingo::dynamic_container_traits {
  using index_definition_type =
      std::tuple<std::tuple<std::size_t, dingo::index_type::array<2>>>;
};

int main() {
  dingo::container<traits> container;
  container.register_indexed_type<dingo::scope<dingo::shared>,
                                  dingo::storage<processor>,
                                  dingo::interfaces<processor>>(std::size_t(1));
}

// CHECK: legacy std::tuple index definitions are no longer supported; use
// dingo::indexes<dingo::index<Interface, Key, Backend>>
