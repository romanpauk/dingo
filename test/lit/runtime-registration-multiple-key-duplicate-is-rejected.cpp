// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct processor {
  virtual ~processor() = default;
};

struct processor_impl : processor {};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, processor>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<processor_impl>,
                          dingo::interfaces<processor>>(dingo::key<int>{7},
                                                        dingo::key<int>{8});
}

// CHECK: duplicate dingo::key<K>{value} arguments for runtime lookup
// CHECK: registration
