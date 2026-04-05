// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

int main() {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<int[2]>,
        dingo::interfaces<float>>();
}

// CHECK: array registrations require matching element-type interfaces
