// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared_cyclical.h>

struct base {
    virtual ~base() = default;
};

struct derived : virtual base {};

int main() {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::shared_cyclical>,
        dingo::storage<derived>,
        dingo::interfaces<base>>();
}

// CHECK: storage requirements not met
