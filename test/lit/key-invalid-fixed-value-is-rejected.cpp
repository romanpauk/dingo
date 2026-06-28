// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/registration/type_registration.h>

struct structural_key {
    int value;
    constexpr bool operator==(const structural_key&) const = default;
};

struct incompatible_key {
    incompatible_key() = default;
    incompatible_key(structural_key) = delete;
};

using key_type = dingo::key<incompatible_key, structural_key{1}>;

int main() {
    return sizeof(key_type);
}

// CHECK: dingo::key<T, V> requires V to be usable as T{V}
