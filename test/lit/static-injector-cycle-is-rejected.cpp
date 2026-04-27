// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static/injector.h>
#include <dingo/storage/shared.h>

struct a {};
struct b {};

using source =
    dingo::bindings<dingo::bind<dingo::scope<dingo::shared>, dingo::storage<a>,
                                dingo::dependencies<b&>>,
                    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<b>,
                                dingo::dependencies<a&>>>;

[[maybe_unused]] dingo::static_injector<source> instance;

int main() {}

// CHECK: static_injector requires an acyclic compile-time binding graph
