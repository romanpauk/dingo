#include <dingo/container.h>
#include <memory>

int main() {
    using namespace dingo;
    ////
    // Define a container with user-provided allocator type
    std::allocator<char> alloc;
    container<dingo::dynamic_container_traits, std::allocator<char>> container(
        alloc);
    ////
}
