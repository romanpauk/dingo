// A static binding constructs its object from its type alone, so a callable
// factory, which is an object, is rejected in static bindings when the
// container type is formed.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: declares no factory; a callable factory is the argument of a
// registration

#include <dingo/factory/callable.h>
#include <dingo/static_container.h>
#include <dingo/storage/unique.h>

auto factory = dingo::callable<int()>([] { return 7; });

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<int>,
                dingo::factory<decltype(factory)>>>;

int main() {
  dingo::static_container<source> instance;
  (void)instance;
}
