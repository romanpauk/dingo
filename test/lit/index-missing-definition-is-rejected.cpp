// The key value of a registration is the key of every interface the binding
// declares, whether or not the lookups declare its domain, so a key of an
// interface without a declaration is accepted and finds the binding.
// (behavior change keys_apply_to_every_interface; the baseline rejected it)
// REQUIRES: linux, can-run-built-executables
// RUN: %dingo_cxx %s -o %t && %t

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {};
struct animal {};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, animal>>;
};

int main() {
  dingo::container<traits> container;
  container
      .register_type<dingo::scope<dingo::shared>, dingo::storage<processor>,
                     dingo::interfaces<processor>>(
          dingo::key_value{std::size_t(1)});
  processor &keyed = container.resolve<processor &>(std::size_t(1));
  (void)keyed;
  try {
    (void)container.resolve<processor &>();
    return 1;
  } catch (const dingo::type_not_found_exception &) {
  }
  return 0;
}
