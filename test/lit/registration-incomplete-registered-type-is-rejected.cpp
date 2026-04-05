// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct service_interface {
    virtual ~service_interface() = default;
};

struct incomplete_service;

int main() {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::unique>,
        dingo::storage<incomplete_service*>,
        dingo::interfaces<service_interface>>(
        dingo::callable([]() -> incomplete_service* { return nullptr; }));
}

// CHECK: registered types must be complete
