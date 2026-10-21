// A local registration takes a key value for every lookup of its interface by
// a runtime key that the traits of the container declare, when it takes one
// for any, as a registration into the container does.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: the registration of

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/storage/shared.h>

struct service {
  virtual ~service() = default;
  virtual int run() const = 0;
};

template <int Id> struct implementation : service {
  int run() const override { return Id; }
};

struct consumer {
  explicit consumer(service &) {}
};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, service>,
                     dingo::associative<long, service>>;
};

int main() {
  dingo::container<traits> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<consumer>>()
      ->register_type<dingo::scope<dingo::shared>,
                      dingo::storage<implementation<1>>,
                      dingo::interfaces<service>>(dingo::key_value{7});
}
