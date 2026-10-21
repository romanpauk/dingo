// A static child shadows its parent: two bindings of a key in the child make
// a singular request by that key ambiguous, though the parent binds the key
// once, and the request is rejected when it is compiled.
// REQUIRES: linux
// RUN: not %dingo_next_cxx -fsyntax-only %s 2>&1 | %filecheck %s
// CHECK: type resolution is ambiguous:

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
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 0>>>;

using child_source = dingo::bindings<
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<first_child_processor>,
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 0>>,
    dingo::bind<dingo::scope<dingo::shared>,
                dingo::storage<second_child_processor>,
                dingo::interfaces<processor>, dingo::key_type<std::size_t, 0>>>;

struct traits : dingo::static_container_traits {
  using lookup_definition_type =
      dingo::lookups<dingo::associative<std::size_t, processor, dingo::many>>;
};

int main() {
  dingo::static_container<parent_source, traits> parent;
  dingo::static_container<child_source, decltype(parent)> child(&parent);
  (void)child.resolve<
      dingo::dependency<processor &, dingo::key_type<std::size_t, 0>>>();
  return 0;
}
