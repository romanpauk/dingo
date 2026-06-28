// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/unique.h>

struct dependency {};

struct service {
  explicit service(dependency &) {}
};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<service>>>;

int main() {
  dingo::static_container<source> instance;
  (void)instance;
}

// clang-format off
// CHECK: {{(bindings<...> source requires every inferred constructor dependency to map to an interface binding|static_container requires a resolvable compile-time binding graph)}}
// clang-format on
