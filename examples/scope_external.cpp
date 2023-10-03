#include <dingo/container.h>
#include <dingo/storage/external.h>

////
struct A {
} instance;

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    // Register existing instance of A, stored as a pointer.
    container.register_type<scope<external>, storage<A*>>(&instance);
    // Resolution will return an existing instance of A casted to required type.
    assert(&container.resolve<A&>() == container.resolve<A*>());
    ////
}
