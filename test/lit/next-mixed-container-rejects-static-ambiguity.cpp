// Two static bindings of a key make a request by that key ambiguous whatever
// a mixed container registers at runtime, so the request is rejected when it
// is compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/lookup/lookup.h>
#include <dingo/storage/shared.h>

struct iface {
  virtual ~iface() = default;
};
template <int Id> struct impl : iface {};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<int, iface, dingo::many>>;
};

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<impl<1>>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<impl<2>>,
                  dingo::interfaces<iface>, dingo::key_type<int, 7>>>;
  dingo::container<bindings, traits> container;
  (void)container.resolve<iface &>(dingo::key_type<int, 7>{});
}

// CHECK: static_container cannot resolve an ambiguously bound type
