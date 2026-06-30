// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct animal {
  virtual ~animal() = default;
};

struct dog : animal {};

struct traits : dingo::dynamic_container_traits {
  using query_definition_type =
      dingo::queries<dingo::single<animal>, dingo::collection<animal>>;
};

int main() {
  dingo::container<traits> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<dog>,
                          dingo::interfaces<animal>>();
}

// CHECK: conflicting dingo query definitions for interface/key domain
