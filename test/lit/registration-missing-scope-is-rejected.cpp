// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/registration/type_registration.h>

using registration = dingo::type_registration<dingo::storage<int>>;
using scope_type = typename registration::scope_type;

int main() {
    return sizeof(scope_type);
}

// CHECK: failed to deduce a scope type
