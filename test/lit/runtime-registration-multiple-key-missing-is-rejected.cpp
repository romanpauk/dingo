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
      dingo::lookups<dingo::associative<int, processor>,
                     dingo::associative<std::string, processor>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>,
                          dingo::storage<processor_impl>,
                          dingo::interfaces<processor>>(dingo::key_value{7});
}

// CHECK: runtime-key lookup registration requires a runtime key value
// CHECK: for each declared runtime-key lookup
