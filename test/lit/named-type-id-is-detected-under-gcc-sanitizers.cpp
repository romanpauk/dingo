// GCC rejects comparing the addresses of distinct objects in a constant
// expression under its null sanitizer, a compiler defect. The library detects
// it and compares type identities by name in constant evaluation, so a static
// container compiles.
// REQUIRES: linux, gcc
// RUN: %dingo_cxx -fsyntax-only -fsanitize=null %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

struct service {};
struct other {};

static_assert(dingo::core::named_type_ids);
static_assert(dingo::core::type_id_v<service> != dingo::core::type_id_v<other>);

int main() {
  using bindings = dingo::bindings<
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<service>>,
      dingo::bind<dingo::scope<dingo::shared>, dingo::storage<other>>>;
  dingo::static_container<bindings> container;
  service &resolved = container.resolve<service &>();
  (void)resolved;
}
