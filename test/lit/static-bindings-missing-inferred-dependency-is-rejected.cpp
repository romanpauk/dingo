// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/unique.h>

struct dependency {};

struct service {
    explicit service(dependency&) {}
};

using source =
    dingo::bindings<dingo::bind<dingo::scope<dingo::unique>, dingo::storage<service>>>;

[[maybe_unused]] dingo::static_container<source> instance;

int main() {}

// CHECK: {{(bindings<...> source requires every inferred constructor dependency to map to an interface binding|static_container requires an acyclic compile-time binding graph)}}
