// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/registration/type_registration.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct service {};

using registration =
    dingo::type_registration<dingo::scope<dingo::shared>,
                             dingo::storage<service>,
                             dingo::key<std::size_t, 0>>;
using key_type = typename registration::key_type;

int main() {
    return sizeof(key_type);
}

// CHECK: dingo::key<T, V> cannot be used as a typed-key registration key
