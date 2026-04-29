// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>

int main() {
    dingo::container<> container;
    container.register_type<dingo::scope<dingo::unique>, dingo::storage<int>>();
}

// CHECK: registered storage tag must be complete
