// RUN: %dingo_cxx -c %s -o %t.o

#include <dingo/registration/type_registration.h>

struct structural_key {
  int value;
  constexpr bool operator==(const structural_key &) const = default;
};

using key_type = dingo::key<structural_key, structural_key{1}>;

int main() {
  auto value = dingo::detail::key_value_traits<key_type>::make();
  return value.value == 1 ? 0 : 1;
}
