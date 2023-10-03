#include <dingo/container.h>
#include <dingo/factory/function.h>
#include <dingo/storage/unique.h>

////
// Declare struct A that has an inaccessible constructor
struct A {
    // Provide factory function to construct instance of A
    static A factory() { return A(); }

  private:
    A() = default;
};

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    // Register A that will be instantiated by calling A::factory()
    container.register_type<scope<unique>, storage<A>, factory<function<&A::factory>>>();
    ////
}
