// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <string>

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
                          dingo::interfaces<processor>>(
      dingo::key<int>{7}, dingo::key<std::string>{"extra"});
}

// CHECK: supplied dingo::key<K>{value} has no matching runtime-key lookup
// CHECK: definition for the registered interface

