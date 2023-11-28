//
// This file is part of dingo project <https://github.com/romanpauk/dingo>
//
// See LICENSE for license and copyright information
// SPDX-License-Identifier: MIT
//

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
