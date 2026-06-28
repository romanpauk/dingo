// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

struct base {
  virtual ~base() = default;
};

struct left : base {};
struct right : base {};

struct service {
  explicit service(base &) {}
};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<left>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<right>>,
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<service>>>;

int main() {
  dingo::static_container<source> instance;
  (void)instance;
}

// CHECK: bindings<...> source requires every inferred constructor dependency to
// map to exactly one interface binding
