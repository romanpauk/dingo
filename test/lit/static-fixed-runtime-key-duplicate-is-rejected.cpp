// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {
  virtual ~processor() = default;
};

struct first_processor : processor {};
struct second_processor : processor {};

using source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<first_processor>,
                dingo::interfaces<processor>, dingo::key<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<second_processor>,
                dingo::interfaces<processor>,
                dingo::key<std::size_t, std::size_t{0}>>>;

struct traits : dingo::static_container_traits<> {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, processor>>;
};

int main() {
  dingo::static_container<source, traits> container;
  (void)container;
  return 0;
}

// CHECK: static_container fixed runtime-key lookup bindings must be
// CHECK: unique for one lookups and unique by storage for many lookups
