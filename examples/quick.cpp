// Include required dingo headers
#include <dingo/container.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

int main() {
    using namespace dingo;
    ////
    // Class types to be managed by the container. Note that there is no special
    // code required for a type to be used by the container and that conversions
    // are applied automatically based on an registered type scope.
    struct A {
        A() {}
    };
    struct B {
        B(A&, std::shared_ptr<A>) {}
    };
    struct C {
        C(B*, std::unique_ptr<B>&, A&) {}
    };

    container<> container;
    // Register struct A with a shared scope, stored as std::shared_ptr<A>
    container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();

    // Register struct B with a shared scope, stored as std::unique_ptr<B>
    container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();

    // Register struct C with an unique scope, stored as plain C
    container.register_type<scope<unique>, storage<C>>();

    // Resolving the struct C will recursively instantiate required dependencies
    // (structs A and B) and inject the instances based on their scopes into C.
    // As C is in unique scope, each resolve<C> will return new C instance.
    // As A and B are in shared scope, each C will get the same instances
    // injected.
    C c = container.resolve<C>();

    struct D {
        A& a;
        B* b;
    };

    // Construct an un-managed struct using dependencies from the container
    D d = container.construct<D>();
    ////
}
