// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/registration/type_registration.h>

using registration =
    dingo::type_registration<dingo::scope<int>, dingo::interfaces<int>>;
using storage_type = typename registration::storage_type;

int main() {
    return sizeof(storage_type);
}

// CHECK: failed to deduce a storage type
