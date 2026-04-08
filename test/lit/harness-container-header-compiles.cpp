// RUN: %dingo_cxx -c %s

#include <dingo/container.h>

int main() {
    dingo::container<> container;
    return container.template resolve<int>();
}
