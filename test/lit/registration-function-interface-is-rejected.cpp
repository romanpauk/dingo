// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

int main() {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<int>,
        dingo::interfaces<int()>>();
}

// CHECK: interfaces must not contain function types
