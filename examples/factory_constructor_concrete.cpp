#include <dingo/container.h>
#include <dingo/storage/external.h>

////
struct A {
    A(int);      // Definition is not required as constructor is not called
    A(double) {} // Definition is required as constructor is called
};

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    container.register_type<scope<external>, storage<double>>(1.1);

    // Register class A that will be constructed using manually selected A(double)
    // constructor. Manually disambiguation is required to avoid compile time assertion
    container.register_type<scope<unique>, storage<A>, factory<constructor<A(double)>>>();
    ////
}
