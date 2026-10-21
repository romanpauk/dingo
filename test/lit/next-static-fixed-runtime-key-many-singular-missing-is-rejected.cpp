// A static request by a fixed key that no binding has, though another key of
// the domain is bound, is not found, and it is rejected when it is compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: type not found: {{.*}} (index type:

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {
  virtual ~processor() = default;
};

struct processor_impl : processor {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<processor_impl>,
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 1>>>;

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
