// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct service {};
struct unrelated_interface {
    virtual ~unrelated_interface() = default;
};

int main() {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<service>,
        dingo::interfaces<unrelated_interface>>();
}

// CHECK: registered type must be pointer-convertible to the interface
