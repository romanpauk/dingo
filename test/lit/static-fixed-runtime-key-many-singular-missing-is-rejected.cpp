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
                dingo::interfaces<processor>, dingo::key<std::size_t, 1>>>;

struct traits : dingo::static_container_traits<> {
  using query_definition_type =
      dingo::queries<dingo::associative<std::size_t, processor, dingo::many>>;
};

int main() {
  dingo::static_container<source, traits> container;
  (void)container
      .resolve<dingo::query<processor &, dingo::key<std::size_t, 0>>>();
  return 0;
}

// CHECK: static_container cannot resolve an unbound type
