// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static/registry.h>
#include <dingo/factory/callable.h>
#include <dingo/storage/unique.h>

auto factory = dingo::callable<int()>([] { return 7; });

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::unique>, dingo::storage<int>,
                dingo::factory<decltype(factory)>>>;
using registry_type = typename source::type;

[[maybe_unused]] registry_type instance;

int main() {}

// CHECK: bindings<...> source requires compile-time-bindable factories
