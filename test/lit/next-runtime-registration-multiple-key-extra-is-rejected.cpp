// The key values of a registration are keys of every interface the binding
// declares, whether or not the lookups declare their domain, so a second key
// value of another type is accepted and finds the same binding.
// (behavior change keys_apply_to_every_interface; the baseline rejected it)
// REQUIRES: linux, can-run-built-executables
// RUN: %dingo_next_cxx %s -o %t && %t

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
      dingo::key_value{7}, dingo::key_value{std::string{"extra"}});
  processor &by_value = container.resolve<processor &>(7);
  processor &by_name = container.resolve<processor &>(std::string{"extra"});
  return &by_value == &by_name ? 0 : 1;
}
