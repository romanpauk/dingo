// RUN: not %dingo_cxx -c %s 2>&1 | %filecheck %s

#include <dingo/static_container.h>
#include <dingo/storage/shared.h>

#include <cstddef>

struct processor {
  virtual ~processor() = default;
};

struct parent_processor : processor {};
struct first_child_processor : processor {};
struct second_child_processor : processor {};

using parent_source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>, dingo::storage<parent_processor>,
                dingo::interfaces<processor>, dingo::key<std::size_t, 0>>>;

using child_source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<first_child_processor>,
                dingo::interfaces<processor>, dingo::key<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<second_child_processor>,
                dingo::interfaces<processor>, dingo::key<std::size_t, 0>>>;

struct traits : dingo::static_container_traits<> {
  using index_definition_type =
      dingo::selectors<dingo::associative<processor, std::size_t, dingo::many>>;
};

int main() {
  dingo::static_container<parent_source, traits> parent;
  dingo::static_container<child_source, decltype(parent)> child(&parent);
  (void)
      child.resolve<dingo::indexed<processor &, dingo::key<std::size_t, 0>>>();
  return 0;
}

// CHECK: static_container cannot resolve an ambiguously bound type
