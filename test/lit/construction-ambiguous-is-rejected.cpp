// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>

struct ambiguous_type {
    ambiguous_type(int) {}
    ambiguous_type(float) {}
};

int main() {
    dingo::container<> container;
    [[maybe_unused]] auto value = container.construct<ambiguous_type>();
}

// CHECK: class T construction not detected or ambiguous
