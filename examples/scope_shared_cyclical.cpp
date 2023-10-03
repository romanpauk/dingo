#include <dingo/container.h>
#include <dingo/storage/shared_cyclical.h>

////
// Forward-declare structs that have cyclical dependency
struct A;
struct B;

// Declare struct A, note that its constructor is taking arguments of struct B
struct A {
    A(B& b, std::shared_ptr<B> bptr) : b_(b), bptr_(bptr) {}
    B& b_;
    std::shared_ptr<B> bptr_;
};

// Declare struct B, note that its constructor is taking arguments of struct A
struct B {
    B(A& a, std::shared_ptr<A> aptr) : a_(a), aptr_(aptr) {}
    A& a_;
    std::shared_ptr<A> aptr_;
};

////
int main() {
    using namespace dingo;
    ////
    container<> container;

    // Register struct A with cyclical scope
    container.register_type<scope<shared_cyclical>, storage<std::shared_ptr<A>>>();
    // Register struct B with cyclical scope
    container.register_type<scope<shared_cyclical>, storage<std::shared_ptr<B>>>();

    // Returns instance of A that has correctly set b_ member to an instance of B,
    // and instance of B has correctly set a_ member to an instance of A. Conversions
    // are supported with cycles, too.
    A& a = container.resolve<A&>();
    B& b = container.resolve<B&>();
    // Check that the instances are constructed as promised
    assert(&a.b_ == &b);
    assert(&a.b_ == a.bptr_.get());
    assert(&b.a_ == &a);
    assert(&b.a_ == b.aptr_.get());
    ////
}
