// The value of a fixed key is constructed as its key type, so a registration
// by a fixed key whose value does not construct one is rejected when it is
// compiled.
// REQUIRES: linux
// RUN: not %dingo_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: the value of a dingo::key_type does not construct its key type

#include <dingo/container.h>
#include <dingo/storage/shared.h>

struct structural_key {
  int value;
  constexpr bool operator==(const structural_key &) const = default;
};

struct incompatible_key {
  incompatible_key() = default;
  incompatible_key(structural_key) = delete;
};

using key_type = dingo::key_type<incompatible_key, structural_key{1}>;

struct service {};

int main() {
  dingo::container<> container;
  container.register_type<dingo::scope<dingo::shared>, dingo::storage<service>,
                          key_type>();
  return sizeof(key_type);
}
