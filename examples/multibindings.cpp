#include <dingo/container.h>
#include <dingo/index/array.h>
#include <dingo/storage/shared.h>

////
struct IProcessor {
    virtual ~IProcessor() {}
};

template <size_t N> struct Processor : IProcessor {};

////
int main() {
    using namespace dingo;
    ////
    container<> container;
    // Register multi-bindings collection
    container.template register_type_collection<
        scope<shared>, storage<std::vector<IProcessor*>>>();

    // Register types under the same interface
    container.template register_type<scope<shared>, storage<Processor<0>>,
                                     interface<IProcessor>>();
    container.template register_type<scope<shared>, storage<Processor<1>>,
                                     interface<IProcessor>>();

    // Resolve the collection
    container.template resolve<std::vector<IProcessor*>>();
    ////
}
