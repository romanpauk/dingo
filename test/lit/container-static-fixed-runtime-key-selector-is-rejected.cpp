// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {
  virtual ~processor() = default;
};

struct processor_impl : processor {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<processor_impl>,
                dingo::interfaces<processor>, dingo::key<std::size_t, 0>>>;

int main() {
  dingo::container<source> container;
  (void)container;
  return 0;
}

// CHECK: container fixed dingo::key<Key, Value> bindings require
// CHECK: static_container with associative<Key, Interface, one>
// CHECK: associative<Key, Interface, many>
