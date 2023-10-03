#include <dingo/container.h>
#include <dingo/storage/unique.h>

struct A {};

int main() {
    using namespace dingo;
    {
        ////
        container<> container;
        // Registration of a struct A
        container.register_type<scope<unique>,               // using unique scope
                                factory<constructor<A>>,     // using constructor-detecting factory
                                storage<std::unique_ptr<A>>, // stored as unique_ptr<A>
                                interface<A>                 // resolvable as A
                                >();
        ////
    }

    {
        container<> container;
        ////
        // As some policies can be deduced from the others, the above registration simplified
        container.register_type<scope<unique>, storage<std::unique_ptr<A>>>();
        ////
    }
}
