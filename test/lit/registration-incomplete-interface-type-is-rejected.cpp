// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct service {};
struct incomplete_interface;

int main() {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<service>,
        dingo::interfaces<incomplete_interface>>();
}

// CHECK: registered interfaces must be complete
