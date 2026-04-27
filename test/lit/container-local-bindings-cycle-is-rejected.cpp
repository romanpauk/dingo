// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct b;

struct a {
    explicit a(b&) {}
};

struct b {
    explicit b(a&) {}
};

int main() {
    dingo::container<> container;
    container.register_type<
        dingo::scope<dingo::shared>, dingo::storage<int>,
        dingo::bindings<
            dingo::bind<dingo::scope<dingo::shared>, dingo::storage<a>,
                        dingo::dependencies<b&>>,
            dingo::bind<dingo::scope<dingo::shared>, dingo::storage<b>,
                        dingo::dependencies<a&>>>>();
}

// CHECK: register_type bindings<...> requires an acyclic compile-time binding graph
