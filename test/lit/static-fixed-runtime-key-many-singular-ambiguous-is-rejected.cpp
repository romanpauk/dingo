// Two static bindings of a fixed key of a domain declared many make a
// singular request by that key ambiguous, and it is rejected when it is
// compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: type resolution is ambiguous:

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {
  virtual ~processor() = default;
};

struct first_processor : processor {};
struct second_processor : processor {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<first_processor>,
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<second_processor>,
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 0>>>;

struct traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, processor, dingo::many>>;
};

int main() {
  dingo::static_container<source, traits> container;
  (void)container.resolve<
      dingo::dependency<processor &, dingo::key_type<std::size_t, 0>>>();
  return 0;
}
