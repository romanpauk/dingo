#include <dingo/factory/constructor.h>

#include <array>

struct empty_aggregate {};

struct dependency {};

struct arbitrary_class {
  arbitrary_class(dependency &, int) {}
};

using dependency_array = std::array<dependency, 8>;

static_assert(dingo::constructor<int>::arity == 0);
static_assert(dingo::constructor<empty_aggregate>::arity == 0);
static_assert(dingo::constructor_detection<dependency_array>::arity == 8);
static_assert(dingo::constructor_detection<arbitrary_class>::arity == 2);

int main() { return 0; }
