// A dense array never stores a key value outside its bound, so a static
// binding of one is rejected when the container type is formed.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s

#include <dingo/lookup/lookup.h>
#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct iface {
  virtual ~iface() = default;
};
struct impl : iface {};

struct traits : dingo::static_container_traits {
  using lookup_definition_type = dingo::lookups<
      dingo::associative<std::size_t, iface, dingo::one, dingo::array<2>>>;
};

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<impl>,
                  dingo::interfaces<iface>, dingo::key_type<std::size_t, 2>>>;
  dingo::static_container<bindings, traits> container;
  (void)container;
}

// CHECK: type index out of range: key type
