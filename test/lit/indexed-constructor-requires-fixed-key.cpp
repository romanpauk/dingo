// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {
  virtual ~processor() = default;
};

struct consumer {
  explicit consumer(dingo::indexed<processor &, dingo::key<std::size_t>>) {}
};

struct traits : dingo::dynamic_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<processor, std::size_t>>;
};

int main() {
  dingo::container<traits> container;
  (void)container.construct<consumer>();
}

// CHECK: dingo::indexed<T, dingo::key<Key>> constructor injection requires
// dingo::key<Key, Value>
