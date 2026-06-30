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
                dingo::interfaces<processor>, dingo::key<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<processor_impl>,
                dingo::interfaces<processor>,
                dingo::key<std::size_t, std::size_t{0}>>>;

struct traits : dingo::static_container_traits<> {
  using index_definition_type =
      dingo::selectors<dingo::associative<processor, std::size_t, dingo::many>>;
};

int main() {
  dingo::static_container<source, traits> container;
  (void)container;
  return 0;
}

// CHECK: static_container fixed runtime-key selector bindings must be
// CHECK: unique for one selectors and unique by storage for many selectors
