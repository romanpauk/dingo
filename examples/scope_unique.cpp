#include <dingo/container.h>
#include <dingo/storage/unique.h>

int main() {
    using namespace dingo;
    ////
    struct A {};
    container<> container;
    // Register struct A with unique scope
    container.register_type<scope<unique>, storage<A>>();
    // Resolution will get a unique instance of A
    container.resolve<A>();
    ////
}
