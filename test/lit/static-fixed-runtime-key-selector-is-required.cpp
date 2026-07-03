// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {
  virtual ~processor() = default;
};

struct processor_impl : processor {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<processor_impl>,
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 0>>>;

int main() {
  dingo::static_container<source> container;
  (void)container;
  return 0;
}

// CHECK: static_container fixed dingo::key_type<Key, Value> bindings require
// CHECK: associative<Key, Interface, one>
// CHECK: associative<Key, Interface, many>
