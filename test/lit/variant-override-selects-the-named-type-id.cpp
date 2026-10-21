// Overriding the compiler variant selects the named type identity, so it is
// compiled and run on compilers without the defect.
// REQUIRES: linux, can-run-built-executables
// RUN: %dingo_cxx -DDINGO_VARIANT_NAMED_TYPE_ID=1 %s -o %t
// RUN: %t

#include <dingo/container.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstdlib>

struct service {};
struct other {};

static_assert(dingo::core::named_type_ids);
static_assert(dingo::core::type_id_v<service> != dingo::core::type_id_v<other>);
static_assert(dingo::core::type_id_v<service> ==
              dingo::core::type_id_v<service>);

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<other>>>;
  dingo::static_container<bindings> fixed;
  dingo::container<> runtime;
  runtime.register_type<dingo::scope<dingo::shared>, dingo::storage<service>>();
  runtime.register_type<dingo::scope<dingo::shared>, dingo::storage<other>>();
  bool distinct = static_cast<void *>(&fixed.resolve<service &>()) !=
                      static_cast<void *>(&fixed.resolve<other &>()) &&
                  static_cast<void *>(&runtime.resolve<service &>()) !=
                      static_cast<void *>(&runtime.resolve<other &>());
  return distinct ? EXIT_SUCCESS : EXIT_FAILURE;
}
