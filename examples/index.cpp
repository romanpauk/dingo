#include <dingo/container.h>
#include <dingo/index/unordered_map.h>
#include <dingo/storage/shared.h>

#include <string>

////
struct IAnimal {
    virtual ~IAnimal() {}
};

struct Dog : IAnimal {};
struct Cat : IAnimal {};

////
int main() {
    using namespace dingo;
    ////
    // Declare traits with std::string based index
    struct container_traits : dynamic_container_traits {
        using index_definition_type =
            std::tuple<std::tuple<std::string, index_type::unordered_map>>;
    };

    container<container_traits> container;
    container.template register_indexed_type<scope<shared>, storage<Dog>,
                                             interface<IAnimal>>(
        std::string("dog"));

    container.template register_indexed_type<scope<shared>, storage<Cat>,
                                             interface<IAnimal>>(
        std::string("cat"));

    // Resolve an instance of a dog
    auto dog = container.template resolve<IAnimal>(std::string("dog"));
    ////
}
